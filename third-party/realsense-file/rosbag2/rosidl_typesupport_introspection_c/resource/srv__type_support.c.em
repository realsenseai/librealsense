@# Included from rosidl_typesupport_introspection_c/resource/idl__type_support.c.em
@{
TEMPLATE(
    'msg__type_support.c.em',
    package_name=package_name, interface_path=interface_path, message=service.request_message,
    include_directives=include_directives)
}@

@{
TEMPLATE(
    'msg__type_support.c.em',
    package_name=package_name, interface_path=interface_path, message=service.response_message,
    include_directives=include_directives)
}@

@{
from rosidl_cmake import convert_camel_case_to_lower_case_underscore
include_parts = [package_name] + list(interface_path.parents[0].parts) + [
    'detail', convert_camel_case_to_lower_case_underscore(interface_path.stem)]
include_base = '/'.join(include_parts)

header_files = [
    'rosidl_runtime_c/service_type_support_struct.h',
    package_name + '/msg/rosidl_typesupport_introspection_c__visibility_control.h',
    include_base + '__rosidl_typesupport_introspection_c.h',
    'rosidl_typesupport_introspection_c/identifier.h',
    'rosidl_typesupport_introspection_c/service_introspection.h',
]

function_prefix = '__'.join(include_parts) + '__rosidl_typesupport_introspection_c'
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

// this is intentionally not const to allow initialization later to prevent an initialization race
static rosidl_typesupport_introspection_c__ServiceMembers @(function_prefix)__@(service.namespaced_type.name)_service_members = {
  "@('__'.join([package_name] + list(interface_path.parents[0].parts)))",  // service namespace
  "@(service.namespaced_type.name)",  // service name
  // these two fields are initialized below on the first access
  NULL,  // request message
  // @(function_prefix)__@(service.request_message.structure.namespaced_type.name)_message_type_support_handle,
  NULL  // response message
  // @(function_prefix)__@(service.response_message.structure.namespaced_type.name)_message_type_support_handle
};

static rosidl_service_type_support_t @(function_prefix)__@(service.namespaced_type.name)_service_type_support_handle = {
  0,
  &@(function_prefix)__@(service.namespaced_type.name)_service_members,
  get_service_typesupport_handle_function,
};

// Forward declaration of request/response type support functions
const rosidl_message_type_support_t *
ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_introspection_c, @(', '.join([package_name] + list(interface_path.parents[0].parts))), @(service.namespaced_type.name)_Request)();

const rosidl_message_type_support_t *
ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_introspection_c, @(', '.join([package_name] + list(interface_path.parents[0].parts))), @(service.namespaced_type.name)_Response)();

ROSIDL_TYPESUPPORT_INTROSPECTION_C_EXPORT_@(package_name)
const rosidl_service_type_support_t *
ROSIDL_TYPESUPPORT_INTERFACE__SERVICE_SYMBOL_NAME(rosidl_typesupport_introspection_c, @(', '.join([package_name] + list(interface_path.parents[0].parts))), @(service.namespaced_type.name))() {
  if (!@(function_prefix)__@(service.namespaced_type.name)_service_type_support_handle.typesupport_identifier) {
    @(function_prefix)__@(service.namespaced_type.name)_service_type_support_handle.typesupport_identifier =
      rosidl_typesupport_introspection_c__identifier;
  }
  rosidl_typesupport_introspection_c__ServiceMembers * service_members =
    (rosidl_typesupport_introspection_c__ServiceMembers *)@(function_prefix)__@(service.namespaced_type.name)_service_type_support_handle.data;

  if (!service_members->request_members_) {
    service_members->request_members_ =
      (const rosidl_typesupport_introspection_c__MessageMembers *)
      ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_introspection_c, @(', '.join([package_name] + list(interface_path.parents[0].parts))), @(service.namespaced_type.name)_Request)()->data;
  }
  if (!service_members->response_members_) {
    service_members->response_members_ =
      (const rosidl_typesupport_introspection_c__MessageMembers *)
      ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_introspection_c, @(', '.join([package_name] + list(interface_path.parents[0].parts))), @(service.namespaced_type.name)_Response)()->data;
  }

  return &@(function_prefix)__@(service.namespaced_type.name)_service_type_support_handle;
}
