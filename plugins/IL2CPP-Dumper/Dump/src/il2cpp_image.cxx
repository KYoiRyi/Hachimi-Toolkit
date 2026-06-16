#include "../include/il2cpp_image.hxx"
#include "../include/il2cpp_api.hxx"


const char * Il2CppImage::GetName( ) const {
    if (!image || !IsValidPointer(image)) return "Unknown";
    return api::image_get_name && image ? api::image_get_name( image ) : "Unknown";
}


size_t Il2CppImage::GetClassCount( ) const {
    if (!image || !IsValidPointer(image)) return 0;
    return api::image_get_class_count && image ? api::image_get_class_count( image ) : 0;
}


Il2CppClass Il2CppImage::GetClassByIndex( size_t index ) const {
    if (!image || !IsValidPointer(image)) return Il2CppClass(nullptr);
    void * k = api::image_get_class && image ? api::image_get_class( image, index ) : nullptr;
    return Il2CppClass( k );
}