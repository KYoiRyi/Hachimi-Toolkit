use std::{
    collections::HashSet,
    ffi::{CStr, CString},
    fs,
    io::Write,
    os::unix::io::FromRawFd,
    path::{Path, PathBuf},
};

use crate::core::{plugin_api::Plugin, Hachimi};

#[cfg(target_arch = "aarch64")]
const SYS_MEMFD_CREATE: libc::c_long = 279;
#[cfg(target_arch = "arm")]
const SYS_MEMFD_CREATE: libc::c_long = 385;
#[cfg(target_arch = "x86_64")]
const SYS_MEMFD_CREATE: libc::c_long = 319;
#[cfg(target_arch = "x86")]
const SYS_MEMFD_CREATE: libc::c_long = 356;
#[cfg(not(any(target_arch = "aarch64", target_arch = "arm", target_arch = "x86_64", target_arch = "x86")))]
const SYS_MEMFD_CREATE: libc::c_long = libc::SYS_memfd_create;

pub fn load_libraries() -> Vec<Plugin> {
    let mut plugins = Vec::new();
    let mut loaded = HashSet::new();
    let config = Hachimi::instance().config.load();
    let names = &config.android.load_libraries;

    if names.is_empty() {
        if let Some(lib_dir) = find_native_lib_dir() {
            for entry in collect_candidate_libs(&lib_dir) {
                let display = entry.display().to_string();
                if loaded.contains(&display) {
                    continue;
                }
                if let Some(plugin) = try_load_library(&display) {
                    loaded.insert(display);
                    plugins.push(plugin);
                }
            }
        } else {
            warn!("Failed to locate native lib dir for plugin autoscan");
        }

        // Auto-scan external plugins directory
        let ext_dir = get_external_plugins_dir();
        if ext_dir.exists() {
            if let Ok(entries) = fs::read_dir(&ext_dir) {
                for entry in entries.flatten() {
                    let path = entry.path();
                    if path.is_file() {
                        if let Some(ext) = path.extension() {
                            if ext == "so" {
                                let path_str = path.to_string_lossy().into_owned();
                                if loaded.contains(&path_str) {
                                    continue;
                                }
                                if let Some(plugin) = try_load_library(&path_str) {
                                    loaded.insert(path_str);
                                    plugins.push(plugin);
                                }
                            }
                        }
                    }
                }
            }
        }
    } else {
        for name in names.iter() {
            if loaded.contains(name) {
                continue;
            }
            if let Some(plugin) = try_load_library(name) {
                loaded.insert(name.clone());
                plugins.push(plugin);
            }
        }
    }

    plugins
}

fn get_external_plugins_dir() -> PathBuf {
    let package_name = crate::android::game_impl::get_package_name();
    let mut path = Path::new("/sdcard/Android/data").join(package_name);
    path.push("files");
    path.push("hachimi");
    path.push("plugins");
    path
}

fn get_internal_cache_dir() -> Option<PathBuf> {
    let vm = crate::android::main::java_vm()?;
    let mut env = vm.attach_current_thread().ok()?;
    let activity = crate::android::utils::get_activity(unsafe { env.unsafe_clone() })?;
    
    let cache_dir_file = env
        .call_method(&activity, "getCacheDir", "()Ljava/io/File;", &[])
        .ok()?
        .l()
        .ok()?;
    
    let path_java = env
        .call_method(&cache_dir_file, "getAbsolutePath", "()Ljava/lang/String;", &[])
        .ok()?
        .l()
        .ok()?;
    
    let path_jstr: jni::objects::JString = path_java.into();
    let path_rust: String = env.get_string(&path_jstr).ok()?.into();
    Some(PathBuf::from(path_rust))
}

fn dlopen_compat(path: &Path) -> Option<*mut libc::c_void> {
    let path_str = path.to_string_lossy();
    
    // 1. Try direct dlopen first (works if in system path or pre-Android 10)
    let Ok(path_cstr) = CString::new(path_str.as_ref()) else {
        return None;
    };
    let handle = unsafe { libc::dlopen(path_cstr.as_ptr(), libc::RTLD_NOW) };
    if !handle.is_null() {
        return Some(handle);
    }
    
    let err = unsafe { libc::dlerror() };
    let err_str = if !err.is_null() {
        unsafe { CStr::from_ptr(err).to_string_lossy().into_owned() }
    } else {
        "Unknown error".to_string()
    };
    debug!("Direct dlopen failed for {}: {}. Trying compatibility loader.", path_str, err_str);

    // 2. Read the file content into memory
    let file_bytes = match fs::read(path) {
        Ok(bytes) => bytes,
        Err(e) => {
            warn!("Failed to read plugin file {}: {}", path_str, e);
            return None;
        }
    };

    // 3. Try memfd_create bypass (Android 10+ anonymous memory execution)
    let fd = unsafe {
        libc::syscall(
            SYS_MEMFD_CREATE,
            c"hachimi_plugin".as_ptr(),
            libc::MFD_CLOEXEC,
        ) as libc::c_int
    };

    if fd >= 0 {
        let mut file = unsafe { fs::File::from_raw_fd(fd) };
        if let Ok(()) = file.write_all(&file_bytes) {
            let proc_path = format!("/proc/self/fd/{}", fd);
            if let Ok(proc_path_cstr) = CString::new(proc_path) {
                let handle = unsafe { libc::dlopen(proc_path_cstr.as_ptr(), libc::RTLD_NOW) };
                if !handle.is_null() {
                    info!("Successfully loaded plugin {} via memfd_create bypass", path_str);
                    return Some(handle);
                }
            }
        }
    }

    // 4. Fallback: Copy to app's cache directory (Android < 10 fallback)
    if let Some(cache_dir) = get_internal_cache_dir() {
        if let Some(file_name) = path.file_name() {
            let dest_path = cache_dir.join(file_name);
            if let Ok(()) = fs::create_dir_all(&cache_dir) {
                if let Ok(()) = fs::write(&dest_path, &file_bytes) {
                    let dest_str = dest_path.to_string_lossy();
                    if let Ok(dest_cstr) = CString::new(dest_str.as_ref()) {
                        let handle = unsafe { libc::dlopen(dest_cstr.as_ptr(), libc::RTLD_NOW) };
                        let _ = fs::remove_file(&dest_path);
                        if !handle.is_null() {
                            info!("Successfully loaded plugin {} via cache fallback", path_str);
                            return Some(handle);
                        }
                    }
                }
            }
        }
    }

    warn!("All loading methods failed for plugin: {}", path_str);
    None
}

fn try_load_library(name_or_path: &str) -> Option<Plugin> {
    let path = Path::new(name_or_path);
    
    let resolved_path = if path.is_absolute() {
        Some(path.to_path_buf())
    } else {
        let ext_dir = get_external_plugins_dir();
        let ext_path = ext_dir.join(name_or_path);
        if ext_path.exists() {
            Some(ext_path)
        } else if let Some(lib_dir) = find_native_lib_dir() {
            let native_path = lib_dir.join(name_or_path);
            if native_path.exists() {
                Some(native_path)
            } else {
                None
            }
        } else {
            None
        }
    };

    let handle = if let Some(ref p) = resolved_path {
        dlopen_compat(p)?
    } else {
        let Ok(name_cstr) = CString::new(name_or_path) else {
            warn!("Invalid library name: {}", name_or_path);
            return None;
        };
        let h = unsafe { libc::dlopen(name_cstr.as_ptr(), libc::RTLD_NOW) };
        if h.is_null() {
            return None;
        }
        h
    };

    let init_enum = {
        let v3_addr = unsafe { libc::dlsym(handle, c"hachimi_init_v3".as_ptr()) };
        if !v3_addr.is_null() {
            Some(crate::core::plugin_api::PluginInit::V3(unsafe { std::mem::transmute(v3_addr) }))
        } else {
            let v2_addr = unsafe { libc::dlsym(handle, c"hachimi_init".as_ptr()) };
            if !v2_addr.is_null() {
                Some(crate::core::plugin_api::PluginInit::V2(unsafe { std::mem::transmute(v2_addr) }))
            } else {
                None
            }
        }
    };

    match init_enum {
        Some(init_fn) => {
            info!("Loaded library: {}", name_or_path);
            Some(Plugin {
                name: name_or_path.to_string(),
                init_fn,
            })
        }
        None => {
            warn!("Library loaded but missing hachimi_init: {}", name_or_path);
            unsafe {
                libc::dlclose(handle);
            }
            None
        }
    }
}

fn find_native_lib_dir() -> Option<PathBuf> {
    let maps = fs::read_to_string("/proc/self/maps").ok()?;
    for line in maps.lines() {
        let Some(path) = line.split_whitespace().last() else {
            continue;
        };
        if path.ends_with("/libmain.so") {
            return Path::new(path).parent().map(Path::to_path_buf);
        }
    }
    None
}

const AUTOSCAN_PREFIX: &str = "libhachimi_";

fn collect_candidate_libs(lib_dir: &Path) -> Vec<PathBuf> {
    let mut libs = Vec::new();
    let Ok(entries) = fs::read_dir(lib_dir) else {
        return libs;
    };
    for entry in entries.flatten() {
        let path = entry.path();
        let Some(file_name) = path.file_name().and_then(|v| v.to_str()) else {
            continue;
        };
        if !file_name.starts_with(AUTOSCAN_PREFIX) || !file_name.ends_with(".so") {
            continue;
        }
        libs.push(path);
    }
    libs
}
