@# Included from rosidl_typesupport_introspection_c/resource/idl__rosidl_typesupport_introspection_c.h.em
@{
TEMPLATE(
    'msg__rosidl_typesupport_introspection_c.h.em',
    package_name=package_name, interface_path=interface_path, message=service.request_message,
    include_directives=include_directives)
}@

@{
TEMPLATE(
    'msg__rosidl_typesupport_introspection_c.h.em',
    package_name=package_name, interface_path=interface_path, message=service.response_message,
    include_directives=include_directives)
}@

@{
header_files = [
    'rosidl_runtime_c/service_type_support_struct.h',
    'rosidl_typesupport_interface/macros.h',
    package_name + '/msg/rosidl_typesupport_introspection_c__visibility_control.h',
]
}@
@[for header_file in header_files]@
@[    if header_file in include_directives]@
// already included above
// @
@[    else]@
@{include_directives.add(header_file)}@
@[    end if]@
#include "@(header_file)"
@[end for]@

ROSIDL_TYPESUPPORT_INTROSPECTION_C_PUBLIC_@(package_name)
const rosidl_service_type_support_t *
ROSIDL_TYPESUPPORT_INTERFACE__SERVICE_SYMBOL_NAME(rosidl_typesupport_introspection_c, @(', '.join([package_name] + list(interface_path.parents[0].parts))), @(service.namespaced_type.name))();
