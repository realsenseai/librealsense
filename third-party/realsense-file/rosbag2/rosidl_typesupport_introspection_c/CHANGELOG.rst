^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Changelog for package rosidl_typesupport_introspection_c
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

1.3.1 (2023-05-27)
------------------

1.3.0 (2022-09-20)
------------------

1.2.1 (2021-04-14)
------------------

1.2.0 (2020-12-08)
------------------

1.1.0 (2020-08-17)
------------------

1.0.1 (2020-06-03)
------------------

1.0.0 (2020-05-22)
------------------
* Fix variable suffix in rosidl_export_typesupport_targets (`#483 <https://github.com/ros2/rosidl/issues/483>`_)
* Contributors: Ivan Santiago Paunovic

0.9.2 (2020-05-19)
------------------
* Force extension points to be registered in order (`#485 <https://github.com/ros2/rosidl/issues/485>`_)
* Contributors: Ivan Santiago Paunovic

0.9.1 (2020-05-08)
------------------
* use typesuport targets instead of libraries (`#478 <https://github.com/ros2/rosidl/issues/478>`_)
* Contributors: Dirk Thomas

0.9.0 (2020-04-24)
------------------
* Export missing targets for single typesupport build, avoid exposing build directories in include dirs (`#477 <https://github.com/ros2/rosidl/issues/477>`_)
* Export targets in addition to include directories / libraries (`#471 <https://github.com/ros2/rosidl/issues/471>`_)
* Fix build with single introspection typesupport (`#470 <https://github.com/ros2/rosidl/issues/470>`_)
* Rename rosidl_runtime_c_message_initialization to rosidl_runtime_c__message_initialization (`#464 <https://github.com/ros2/rosidl/issues/464>`_)
* Move non-entry point headers into detail subdirectory (`#461 <https://github.com/ros2/rosidl/issues/461>`_)
* Rename rosidl_generator_c 'namespace' to rosidl_runtime_c (`#458 <https://github.com/ros2/rosidl/issues/458>`_)
* Split rosidl_generator_c and rosidl_generator_cpp in two: rosidl_generator_x and rosidl_runtime_x (`#442 <https://github.com/ros2/rosidl/issues/442>`_)
* Export typesupport library in a separate cmake variable (`#453 <https://github.com/ros2/rosidl/issues/453>`_)
* Style update to match uncrustify with explicit language (`#439 <https://github.com/ros2/rosidl/issues/439>`_)
* Move repeated logic for C include prefix into common function (`#432 <https://github.com/ros2/rosidl/issues/432>`_)
* Contributors: Alejandro Hernández Cordero, Dirk Thomas, Ivan Santiago Paunovic, Jacob Perron

0.8.2 (2020-01-17)
------------------

0.8.1 (2019-10-23)
------------------
* Add init and fini function for creating introspection messages (`#416 <https://github.com/ros2/rosidl/issues/416>`_)
* Contributors: Karsten Knese

0.8.0 (2019-09-24)
------------------
* [rosidl_typesupport_introspection_c] Use message namespaced type name as function prefix (`#387 <https://github.com/ros2/rosidl/issues/387>`_)
* fix cpp generator and introspection ts for long double (`#383 <https://github.com/ros2/rosidl/issues/383>`_)
* Contributors: Dirk Thomas, Jacob Perron
