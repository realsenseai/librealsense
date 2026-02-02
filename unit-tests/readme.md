# Unit-Tests

## Running the Unit-Tests

When running **CMake** please make sure the following flag is passed:
`-DBUILD_UNIT_TESTS=true`
After running `make && sudo make install`, you can execute `live-test` to run library unit-tests. 
Make sure you have Intel® RealSense™ device connected. 

## Testing just the Software

If not all unit-tests are passing this can be related to faulty device or problems with the environment setup. 
We support recording test flows at the backend level and running tests on top of mock hardware. This helps us distinguish between hardware problems and software issues. 

* You can record unit-test into file using:
`./live-test into <filename>`

* To run unit-tests without actual hardware, based on recorded data, run:
`./live-test from <filename>`

This mode of operation lets you test your code on a variety of simulated devices.  

## Test Data

If you would like to run and debug unit-tests locally on your machine but you don't have a RealSense device, we publish a set of *unit-test* recordings. These files capture expected execution of the test-suite over several types of hardware (D415, D435, etc..) 
Please see [Github Actions](https://docs.github.com/en/actions) for the exact URLs. 

> These recordings do not contain any imaging data and therefore can only be useful for unit-tests. If you would like to run your algorithms on top of captured data, please review our [playback and record](https://github.com/realsenseai/librealsense/tree/master/src/media) capabilities. 

In addition to running the tests locally, it is very easy to replicate our continuous integration process for your fork of the project - just sign-in to [Github Actions](https://docs.github.com/en/actions) and enable builds on your fork of `librealsense`. 

## Controlling Test Execution

We are using [Catch](https://github.com/philsquared/Catch) as our test framework. 

To see the list of passing tests (and not just the failures), add `-d yes` to test command line.
