@# Included from rosidl_typesupport_introspection_c/resource/idl__type_support.c.em
@{
from rosidl_cmake import convert_camel_case_to_lower_case_underscore
from rosidl_generator_c import idl_structure_type_to_c_include_prefix
from rosidl_parser.definition import AbstractGenericString
from rosidl_parser.definition import AbstractNestedType
from rosidl_parser.definition import AbstractSequence
from rosidl_parser.definition import AbstractString
from rosidl_parser.definition import AbstractWString
from rosidl_parser.definition import Array
from rosidl_parser.definition import BasicType
from rosidl_parser.definition import BoundedSequence
from rosidl_parser.definition import NamespacedType

include_parts = [package_name] + list(interface_path.parents[0].parts) + [
    'detail', convert_camel_case_to_lower_case_underscore(interface_path.stem)]
include_base = '/'.join(include_parts)

header_files = [
    'stddef.h',  # providing offsetof()
    include_base + '__rosidl_typesupport_introspection_c.h',
    package_name + '/msg/rosidl_typesupport_introspection_c__visibility_control.h',
    'rosidl_typesupport_introspection_c/field_types.h',
    'rosidl_typesupport_introspection_c/identifier.h',
    'rosidl_typesupport_introspection_c/message_introspection.h',
    include_base + '__functions.h',
    include_base + '__struct.h',
]

function_prefix = message.structure.namespaced_type.name + '__rosidl_typesupport_introspection_c'
}@
@[for header_file in header_files]@
@[    if header_file in include_directives]@
// already included above
// @
@[    else]@
@{include_directives.add(header_file)}@
@[    end if]@
@[    if '/' not in header_file]@
#include <@(header_file)>
@[    else]@
#include "@(header_file)"
@[    end if]@
@[end for]@

@#<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
@# Collect necessary include directives for all members
@{
from collections import OrderedDict
includes = OrderedDict()
for member in message.structure.members:
    if isinstance(member.type, AbstractSequence) and isinstance(member.type.value_type, BasicType):
        member_names = includes.setdefault(
            'rosidl_runtime_c/primitives_sequence_functions.h', [])
        member_names.append(member.name)
        continue
    type_ = member.type
    if isinstance(type_, AbstractNestedType):
        type_ = type_.value_type
    if isinstance(type_, AbstractString):
        member_names = includes.setdefault('rosidl_runtime_c/string_functions.h', [])
        member_names.append(member.name)
    elif isinstance(type_, AbstractWString):
        member_names = includes.setdefault(
            'rosidl_runtime_c/u16string_functions.h', [])
        member_names.append(member.name)
    elif isinstance(type_, NamespacedType):
        include_prefix = idl_structure_type_to_c_include_prefix(type_)
        member_names = includes.setdefault(
            include_prefix + '.h', [])
        member_names.append(member.name)
        include_prefix_detail = idl_structure_type_to_c_include_prefix(type_, 'detail')
        member_names = includes.setdefault(
            include_prefix_detail + '__rosidl_typesupport_introspection_c.h', [])
        member_names.append(member.name)
}@
@#>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
@
@#<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
@[if includes]@

// Include directives for member types
@[    for header_file, member_names in includes.items()]@
@[        for member_name in member_names]@
// Member `@(member_name)`
@[        end for]@
@[        if header_file in include_directives]@
// already included above
// @
@[        else]@
@{include_directives.add(header_file)}@
@[        end if]@
#include "@(header_file)"
@[    end for]@
@[end if]@
@#>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

#ifdef __cplusplus
extern "C"
{
#endif

@
@#######################################################################
@# define callback functions
@#######################################################################
void @(function_prefix)__@(message.structure.namespaced_type.name)_init_function(
  void * message_memory, enum rosidl_runtime_c__message_initialization _init)
{
  // TODO(karsten1987): initializers are not yet implemented for typesupport c
  // see https://github.com/ros2/ros2/issues/397
  (void) _init;
  @('__'.join([package_name] + list(interface_path.parents[0].parts) + [message.structure.namespaced_type.name]))__init(message_memory);
}

void @(function_prefix)__@(message.structure.namespaced_type.name)_fini_function(void * message_memory)
{
  @('__'.join([package_name] + list(interface_path.parents[0].parts) + [message.structure.namespaced_type.name]))__fini(message_memory);
}

@[for member in message.structure.members]@
@[  if isinstance(member.type, AbstractNestedType) and isinstance(member.type.value_type, NamespacedType)]@
size_t @(function_prefix)__size_function__@(member.type.value_type.name)__@(member.name)(
  const void * untyped_member)
{
@[    if isinstance(member.type, Array)]@
  (void)untyped_member;
  return @(member.type.size);
@[    else]@
  const @('__'.join(member.type.value_type.namespaced_name()))__Sequence * member =
    (const @('__'.join(member.type.value_type.namespaced_name()))__Sequence *)(untyped_member);
  return member->size;
@[    end if]@
}

const void * @(function_prefix)__get_const_function__@(member.type.value_type.name)__@(member.name)(
  const void * untyped_member, size_t index)
{
@[    if isinstance(member.type, Array)]@
  const @('__'.join(member.type.value_type.namespaced_name())) ** member =
    (const @('__'.join(member.type.value_type.namespaced_name())) **)(untyped_member);
  return &(*member)[index];
@[    else]@
  const @('__'.join(member.type.value_type.namespaced_name()))__Sequence * member =
    (const @('__'.join(member.type.value_type.namespaced_name()))__Sequence *)(untyped_member);
  return &member->data[index];
@[    end if]@
}

void * @(function_prefix)__get_function__@(member.type.value_type.name)__@(member.name)(
  void * untyped_member, size_t index)
{
@[    if isinstance(member.type, Array)]@
  @('__'.join(member.type.value_type.namespaced_name())) ** member =
    (@('__'.join(member.type.value_type.namespaced_name())) **)(untyped_member);
  return &(*member)[index];
@[    else]@
  @('__'.join(member.type.value_type.namespaced_name()))__Sequence * member =
    (@('__'.join(member.type.value_type.namespaced_name()))__Sequence *)(untyped_member);
  return &member->data[index];
@[    end if]@
}

@[    if isinstance(member.type, AbstractSequence)]@
bool @(function_prefix)__resize_function__@(member.type.value_type.name)__@(member.name)(
  void * untyped_member, size_t size)
{
  @('__'.join(member.type.value_type.namespaced_name()))__Sequence * member =
    (@('__'.join(member.type.value_type.namespaced_name()))__Sequence *)(untyped_member);
  @('__'.join(member.type.value_type.namespaced_name()))__Sequence__fini(member);
  return @('__'.join(member.type.value_type.namespaced_name()))__Sequence__init(member, size);
}

@[    end if]@
@[  end if]@
@[end for]@
static rosidl_typesupport_introspection_c__MessageMember @(function_prefix)__@(message.structure.namespaced_type.name)_message_member_array[@(len(message.structure.members))] = {
@{
for index, member in enumerate(message.structure.members):
    type_ = member.type
    if isinstance(type_, AbstractNestedType):
        type_ = type_.value_type

    print('  {')

    # const char * name_
    print('    "%s",  // name' % member.name)
    if isinstance(type_, BasicType):
        # uint8_t type_id_
        print('    rosidl_typesupport_introspection_c__ROS_TYPE_%s,  // type' % type_.typename.replace(' ', '_').upper())
        # size_t string_upper_bound
        print('    0,  // upper bound of string')
        # const rosidl_generator_c::MessageTypeSupportHandle * members_
        print('    NULL,  // members of sub message')
    elif isinstance(type_, AbstractGenericString):
        # uint8_t type_id_
        if isinstance(type_, AbstractString):
            print('    rosidl_typesupport_introspection_c__ROS_TYPE_STRING,  // type')
        elif isinstance(type_, AbstractWString):
            print('    rosidl_typesupport_introspection_c__ROS_TYPE_WSTRING,  // type')
        else:
            assert False, 'Unknown type: ' + str(type_)
        # size_t string_upper_bound
        print('    %u,  // upper bound of string' % (type_.maximum_size if type_.has_maximum_size() else 0))
        # const rosidl_generator_c::MessageTypeSupportHandle * members_
        print('    NULL,  // members of sub message')
    else:
        # uint8_t type_id_
        print('    rosidl_typesupport_introspection_c__ROS_TYPE_MESSAGE,  // type')
        # size_t string_upper_bound
        print('    0,  // upper bound of string')
        # const rosidl_message_type_support_t * members_
        print('    NULL,  // members of sub message (initialized later)')
    # bool is_array_
    print('    %s,  // is array' % ('true' if isinstance(member.type, AbstractNestedType) else 'false'))
    # size_t array_size_
    print('    %u,  // array size' % (member.type.size if isinstance(member.type, Array) else (member.type.maximum_size if isinstance(member.type, BoundedSequence) else 0)))
    # bool is_upper_bound_
    print('    %s,  // is upper bound' % ('true' if isinstance(member.type, BoundedSequence) else 'false'))
    # unsigned long offset_
    print('    offsetof(%s__%s, %s),  // bytes offset in struct' % ('__'.join([package_name] + list(interface_path.parents[0].parts)), message.structure.namespaced_type.name, member.name))
    # void * default_value_
    print('    NULL,  // default value')  # TODO default value to be set

    function_suffix = ('%s__%s' % (member.type.value_type.name, member.name)) if isinstance(member.type, AbstractNestedType) and isinstance(member.type.value_type, NamespacedType) else None

    # size_t(const void *) size_function
    print('    %s,  // size() function pointer' % ('%s__size_function__%s' % (function_prefix, function_suffix) if function_suffix else 'NULL'))
    # const void *(const void *, size_t) get_const_function
    print('    %s,  // get_const(index) function pointer' % ('%s__get_const_function__%s' % (function_prefix, function_suffix) if function_suffix else 'NULL'))
    # void *(void *, size_t) get_function
    print('    %s,  // get(index) function pointer' % ('%s__get_function__%s' % (function_prefix, function_suffix) if function_suffix else 'NULL'))
    # void(void *, size_t) resize_function
    print('    %s  // resize(index) function pointer' % ('%s__resize_function__%s' % (function_prefix, function_suffix) if function_suffix and isinstance(member.type, AbstractSequence) else 'NULL'))

    if index < len(message.structure.members) - 1:
        print('  },')
    else:
        print('  }')
}@
};

static const rosidl_typesupport_introspection_c__MessageMembers @(function_prefix)__@(message.structure.namespaced_type.name)_message_members = {
  "@('__'.join([package_name] + list(interface_path.parents[0].parts)))",  // message namespace
  "@(message.structure.namespaced_type.name)",  // message name
  @(len(message.structure.members)),  // number of fields
  sizeof(@('__'.join([package_name] + list(interface_path.parents[0].parts) + [message.structure.namespaced_type.name]))),
  @(function_prefix)__@(message.structure.namespaced_type.name)_message_member_array,  // message members
  @(function_prefix)__@(message.structure.namespaced_type.name)_init_function,  // function to initialize message memory (memory has to be allocated)
  @(function_prefix)__@(message.structure.namespaced_type.name)_fini_function  // function to terminate message instance (will not free memory)
};

// this is not const since it must be initialized on first access
// since C does not allow non-integral compile-time constants
static rosidl_message_type_support_t @(function_prefix)__@(message.structure.namespaced_type.name)_message_type_support_handle = {
  0,
  &@(function_prefix)__@(message.structure.namespaced_type.name)_message_members,
  get_message_typesupport_handle_function,
};

ROSIDL_TYPESUPPORT_INTROSPECTION_C_EXPORT_@(package_name)
const rosidl_message_type_support_t *
ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_introspection_c, @(', '.join([package_name] + list(interface_path.parents[0].parts) + [message.structure.namespaced_type.name])))() {
@[for i, member in enumerate(message.structure.members)]@
@{
type_ = member.type
if isinstance(type_, AbstractNestedType):
    type_ = type_.value_type
}@
@[    if isinstance(type_, NamespacedType)]@
  @(function_prefix)__@(message.structure.namespaced_type.name)_message_member_array[@(i)].members_ =
    ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_introspection_c, @(', '.join(type_.namespaced_name())))();
@[    end if]@
@[end for]@
  if (!@(function_prefix)__@(message.structure.namespaced_type.name)_message_type_support_handle.typesupport_identifier) {
    @(function_prefix)__@(message.structure.namespaced_type.name)_message_type_support_handle.typesupport_identifier =
      rosidl_typesupport_introspection_c__identifier;
  }
  return &@(function_prefix)__@(message.structure.namespaced_type.name)_message_type_support_handle;
}
#ifdef __cplusplus
}
#endif
