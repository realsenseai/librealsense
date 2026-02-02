# .NET Wrapper for Intel® RealSense™ SDK

This folder offers P/Invoke based bindings for most SDK APIs, together with a couple of examples.

## Table of Contents

* [Building](#building)
  * [With CMake and Visual Studio](#with-cmake-and-visual-studio)
  * [With .NET Core CLI](#with-.net-core-cli)
  * [With Mono](#with-mono)
* [Hello World](#hello-world)
* [Next Steps](#next-steps)

## Building

To work with Intel RealSense from .NET you will need two libraries next to your application - `realsense2` and `Intel.RealSense.dll`.

In order to get the native `realsense2` library you can either build the SDK [from source using CMake](https://github.com/realsenseai/librealsense/blob/master/doc/installation_windows.md) or [install the latest release](https://github.com/realsenseai/librealsense/blob/master/doc/distribution_windows.md).

Next, build the managed class library project:

### With CMake and Visual Studio

> This is the recommended option, you get the library and samples in a single solution,
> developers have both the native and managed projects and can enable native code debugging.

Prerequisites :

* Visual Studio ≥ 2015
* .NET framework ≥ 3.5 (.NET 3.5 is required for the Unity wrapper)

After installing all prerequisites, generate `realsense2.sln` with `BUILD_CSHARP_BINDINGS` and `BUILD_SHARED_LIBS` flags using cmake.

Generate the VS solution using cmake (run from librealsense root dir):

```cmd
mkdir build
cd build
cmake .. -DBUILD_CSHARP_BINDINGS=ON -DBUILD_SHARED_LIBS=ON
```

The file `realsense2.sln` should be created in *build* folder, open the file with Visual Studio, C# examples and library will be available in the solution under `Wrappers/csharp`.

Both the native library and the .NET wrapper are built by default as part of the examples dependencies.

### With .NET Core CLI

Create `csharp\Intel.RealSense\Intel.RealSense.csproj` with following:

```xml
<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <TargetFramework>netcoreapp2.0</TargetFramework>
    <GenerateAssemblyInfo>false</GenerateAssemblyInfo>
  </PropertyGroup>
</Project>
```

and build with:

```cmd
cd csharp\Intel.RealSense
dotnet build
```

### With Mono

[Get Mono](https://www.mono-project.com/download/stable/) for your platform.

Create `csharp\Intel.RealSense\Intel.RealSense.csproj` with following:

```xml
<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup>
    <AssemblyName>Intel.RealSense</AssemblyName>
    <TargetFrameworkVersion>v3.5</TargetFrameworkVersion>
    <OutputType>Library</OutputType>
    <OutputPath>.</OutputPath>
  </PropertyGroup>
  <ItemGroup>
    <Compile Include="**/*.cs" />
    <Reference Include="System" />
  </ItemGroup>
  <Import Project="$(MSBuildBinPath)\Microsoft.CSharp.targets" />
</Project>
```

and build with:

```sh
$ cd csharp/Intel.RealSense
$ msbuild /nologo /verbosity:minimal
  Intel.RealSense -> /mnt/c/tmp/mono/Intel.RealSense/Intel.RealSense.dll
```
> Tested with Mono C# compiler version 5.16.0.220

The project can also be opened and built in MonoDevelop.

## Hello World

Here is a minimal depth application written in C#:

```cs
var pipe = new Pipeline();
pipe.Start();

while (true)
{
    using (var frames = pipe.WaitForFrames())
    using (var depth = frames.DepthFrame)
    {
        Console.WriteLine("The camera is pointing at an object " +
            depth.GetDistance(depth.Width / 2, depth.Height / 2) + " meters away\t");

        Console.SetCursorPosition(0, 0);
    }
}
```

> **Note:** Since the SDK is holding-on to native hardware resources, it is critical to make sure you deterministically dispose of objects, especially those derived from `Frame`. Without releasing resources explicitly the Garbage Collector will not keep-up with new frames being allocated.

## Next steps

See the samples located in this folder, the [cookbook](./Documentation/cookbook.md) and the [pinvoke notes](./Documentation/pinvoke.md).
