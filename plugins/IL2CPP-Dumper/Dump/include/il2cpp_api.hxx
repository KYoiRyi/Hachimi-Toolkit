#ifndef IL2CPP_API_H
#define IL2CPP_API_H

#include <cstddef>
#include <cstdint>

namespace api {

    extern bool initialized;
    extern uintptr_t module_base;
    extern size_t module_size;

    // function pointer types
    typedef void * ( * get_domain_t )( );
    typedef void ** ( * get_assemblies_t )( void * domain, size_t * count );
    typedef void * ( * assembly_get_image_t )( void * assembly );
    typedef const char * ( * image_get_name_t )( void * image );
    typedef size_t( * image_get_class_count_t )( void * image );
    typedef void * ( * image_get_class_t )( void * image, size_t index );

    typedef const char * ( * class_get_name_t )( void * klass );
    typedef const char * ( * class_get_namespace_t )( void * klass );
    typedef uint32_t( * class_get_flags_t )( void * klass );
    typedef void * ( * class_get_parent_t )( void * klass );
    typedef bool( * class_is_valuetype_t )( void * klass );
    typedef bool( * class_is_interface_t )( void * klass );
    typedef void * ( * class_get_interfaces_t )( void * klass, void ** iter );

    typedef size_t( * class_num_fields_t )( void * klass );
    typedef void * ( * class_get_fields_t )( void * klass, void ** iter );
    typedef const char * ( * field_get_name_t )( void * field );
    typedef void * ( * field_get_type_t )( void * field );
    typedef uint32_t( * field_get_flags_t )( void * field );
    typedef int32_t( * field_get_offset_t )( void * field );

    typedef void * ( * class_get_methods_t )( void * klass, void ** iter );
    typedef const char * ( * method_get_name_t )( void * method );
    typedef uint32_t( * method_get_flags_t )( void * method,
        uint32_t * iflags );
    typedef uint32_t( * method_get_param_count_t )( void * method );
    typedef const void * ( * method_get_param_t )( void * method,
        uint32_t index );
    typedef const char * ( * method_get_param_name_t )( void * method,
        uint32_t index );
    typedef void * ( * method_get_return_type_t )( void * method );
    typedef const void * ( * method_get_pointer_t )( void * method );

    typedef const char * ( * type_get_name_t )( void * type );
    typedef uint32_t( * class_get_type_token_t )( void * klass );

    typedef void * ( * thread_attach_t )( void * domain );
    typedef void( * thread_detach_t )( void * thread );

    // Custom attributes
    typedef void * ( * custom_attrs_from_class_t )( void * klass );
    typedef void * ( * custom_attrs_from_method_t )( void * method );
    typedef void * ( * custom_attrs_from_field_t )( void * klass, void * field );
    typedef void( * custom_attrs_free_t )( void * cache );

    // Field metadata
    typedef uint32_t( * field_get_token_t )( void * field );
    typedef void( * field_static_get_value_t )( void * field, void * value );
    // Returns Il2CppType*;
    typedef void * ( * field_get_default_value_t )( void * field,
        void ** data_out );

    // Method metadata
    typedef uint32_t( * method_get_token_t )( void * method );
    typedef bool( * method_is_generic_t )( void * method );
    typedef bool( * method_is_inflated_t )( void * method );

    // Class lookup
    typedef void * ( * class_from_name_t )( void * image, const char * ns,
        const char * name );

    // Class layout
    typedef int32_t( * class_instance_size_t )( void * klass );
    typedef int32_t( * class_value_size_t )( void * klass, uint32_t * align );

    // Properties / events
    typedef void * ( * class_get_properties_t )( void * klass, void ** iter );
    typedef void * ( * class_get_events_t )( void * klass, void ** iter );
    typedef const char * ( * property_get_name_t )( void * prop );
    typedef void * ( * property_get_get_method_t )( void * prop );
    typedef void * ( * property_get_set_method_t )( void * prop );
    typedef uint32_t( * property_get_flags_t )( void * prop );
    typedef const char * ( * event_get_name_t )( void * event );
    typedef void * ( * event_get_add_method_t )( void * event );
    typedef void * ( * event_get_remove_method_t )( void * event );
    typedef void * ( * event_get_raise_method_t )( void * event );

    typedef void * ( * class_get_nested_types_t )( void * klass, void ** iter );

    typedef void * ( * class_get_method_from_name_t )( void * klass,
        const char * name,
        int argc );
    typedef void * ( * runtime_invoke_t )( void * method, void * obj,
        void ** params, void ** exception );
    typedef void * ( * type_get_object_t )( void * type );
    typedef void * ( * class_get_type_t )( void * klass );

    typedef int32_t( * array_length_t )( void * array );
    typedef int32_t( * string_length_t )( void * str );
    typedef wchar_t * ( * string_chars_t )( void * str );
    typedef void * ( * object_unbox_t )( void * obj );

    typedef void( * field_get_value_t )( void * obj, void * field,
        void * value_out );
    typedef void * ( * class_get_field_from_name_t )( void * klass,
        const char * name );

    typedef int( * gc_register_my_thread_t )( const void * stack_base );
    typedef int( * gc_unregister_my_thread_t )( );
    typedef void( * gc_disable_t )( );
    typedef void( * gc_enable_t )( );
    typedef int( * gc_is_disabled_t )( );

    extern get_domain_t get_domain;
    extern get_assemblies_t get_assemblies;
    extern assembly_get_image_t assembly_get_image;
    extern image_get_name_t image_get_name;
    extern image_get_class_count_t image_get_class_count;
    extern image_get_class_t image_get_class;

    extern class_get_name_t class_get_name;
    extern class_get_namespace_t class_get_namespace;
    extern class_get_flags_t class_get_flags;
    extern class_get_parent_t class_get_parent;
    extern class_is_valuetype_t class_is_valuetype;
    extern class_is_interface_t class_is_interface;
    extern class_get_interfaces_t class_get_interfaces;

    extern class_num_fields_t class_num_fields;
    extern class_get_fields_t class_get_fields;
    extern field_get_name_t field_get_name;
    extern field_get_type_t field_get_type;
    extern field_get_flags_t field_get_flags;
    extern field_get_offset_t field_get_offset;

    extern class_get_methods_t class_get_methods;
    extern method_get_name_t method_get_name;
    extern method_get_flags_t method_get_flags;
    extern method_get_param_count_t method_get_param_count;
    extern method_get_param_t method_get_param;
    extern method_get_param_name_t method_get_param_name;
    extern method_get_return_type_t method_get_return_type;
    extern method_get_pointer_t method_get_pointer;

    extern type_get_name_t type_get_name;
    extern class_get_type_token_t class_get_type_token;

    extern thread_attach_t thread_attach;
    extern thread_detach_t thread_detach;

    extern custom_attrs_from_class_t custom_attrs_from_class;
    extern custom_attrs_from_method_t custom_attrs_from_method;
    extern custom_attrs_from_field_t custom_attrs_from_field;
    extern custom_attrs_free_t custom_attrs_free;

    extern field_get_token_t field_get_token;
    extern field_static_get_value_t field_static_get_value;
    extern field_get_default_value_t field_get_default_value;

    extern method_get_token_t method_get_token;
    extern method_is_generic_t method_is_generic;
    extern method_is_inflated_t method_is_inflated;

    extern class_from_name_t class_from_name;
    extern class_instance_size_t class_instance_size;
    extern class_value_size_t class_value_size;

    extern class_get_properties_t class_get_properties;
    extern class_get_events_t class_get_events;
    extern property_get_name_t property_get_name;
    extern property_get_get_method_t property_get_get_method;
    extern property_get_set_method_t property_get_set_method;
    extern property_get_flags_t property_get_flags;
    extern event_get_name_t event_get_name;
    extern event_get_add_method_t event_get_add_method;
    extern event_get_remove_method_t event_get_remove_method;
    extern event_get_raise_method_t event_get_raise_method;

    extern class_get_nested_types_t class_get_nested_types;

    extern class_get_method_from_name_t class_get_method_from_name;
    extern runtime_invoke_t runtime_invoke;
    extern type_get_object_t type_get_object;
    extern class_get_type_t class_get_type;
    extern array_length_t array_length;
    extern string_length_t
        string_length_fn; // _fn to avoid clash with header field name
    extern string_chars_t string_chars;
    extern object_unbox_t object_unbox;
    extern field_get_value_t field_get_value;
    extern class_get_field_from_name_t class_get_field_from_name;

    extern gc_register_my_thread_t gc_register_my_thread;
    extern gc_unregister_my_thread_t gc_unregister_my_thread;
    extern gc_disable_t gc_disable;
    extern gc_enable_t gc_enable;
    extern gc_is_disabled_t gc_is_disabled;

    typedef void( * gc_disable_boehm_t )( );
    typedef void( * gc_enable_boehm_t )( );
    extern gc_disable_boehm_t gc_disable_boehm;
    extern gc_enable_boehm_t gc_enable_boehm;
    extern int * gc_dont_gc_ptr;

    void init( );

}

#endif // IL2CPP_API_H
