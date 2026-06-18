# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

from pyrsutils import shorten_json_string as short
from pytest_check import check


# This tests shorten_json_string, but indirectly also json conversion from python objects


#############################################################################################
#
def test_string():
    # strings are not enforced as valid json objects
    check.equal( short( '' ), '' )
    check.equal( short( '{' ), '{' )
    check.equal( short( '{}' ), '{}' )
    check.equal( short( '[' ), '[' )
    check.equal( short( '[]' ), '[]' )
    check.equal( short( '[{}]' ), '[{}]' )
    check.equal( short( '{[{}]}' ), '{[{}]}' )  # note: invalid dict/json, but OK as a string!
    check.equal( short( '[1,2,3]' ), '[1,2,3]' )
    check.equal( short( '{"some":"one"}' ), '{"some":"one"}' )


#############################################################################################
#
def test_string_max_length():
    #
    # max-length must be reasonably long (at least enough characters to encompass '{ ... }' or 7
    # characters) or the original string will be returned:
    #
    s = '{"something":"great"}'
    check.equal( short( s, 0 ), s )  # invalid
    check.equal( short( s, 1 ), s )  # invalid
    check.equal( short( s, 2 ), s )  # invalid
    check.equal( short( s, 3 ), s )  # invalid
    check.equal( short( s, 4 ), s )  # invalid
    check.equal( short( s, 5 ), s )  # invalid
    check.equal( short( s, 6 ), s )  # invalid
    check.equal( short( s, 7 ), '{ ... }' )
    check.equal( short( s, 10 ), '{ ... }' )  # no break mid-item
    check.equal( short( s, 17 ), '{" ... ":"great"}' )  # strings can be reduced, too...
    #
    s = '[1,2,3,4,5,6,7,8]'
    check.equal( short( s, 6 ), s )
    check.equal( short( s, 7 ), '[ ... ]' )
    check.equal( short( s, 8 ), '[ ... ]' )  # no break mid-item
    check.equal( short( s, 9 ), '[1, ... ]' )  # no break mid-item
    check.equal( short( s, 16 ), '[1,2,3,4, ... ]' )
    check.equal( short( s, 17 ), s )
    check.equal( short( s, 18 ), s )
    #
    # really long string input with no single good removal option
    s = '{"id":"stream-options","intrinsics":[[424,240,213.24244689941406,122.08871459960938,212.90440368652344,212.41697692871094,2,-0.05456758290529251,0.06640911102294922,-0.00016342636081390083,0.0008340515778400004,-0.02172667160630226],[480,270,241.4065399169922,137.3645782470703,241.0238494873047,240.4720458984375,2,-0.05456758290529251,0.06640911102294922,-0.00016342636081390083,0.0008340515778400004,-0.02172667160630226],[640,360,321.8753662109375,183.15277099609375,321.3651123046875,320.62939453125,2,-0.05456758290529251,0.06640911102294922,-0.00016342636081390083,0.0008340515778400004,-0.02172667160630226],[640,480,322.25042724609375,243.7833251953125,385.6381530761719,384.7552490234375,2,-0.05456758290529251,0.06640911102294922,-0.00016342636081390083,0.0008340515778400004,-0.02172667160630226],[848,480,426.4848937988281,244.17742919921875,425.8088073730469,424.8339538574219,2,-0.05456758290529251,0.06640911102294922,-0.00016342636081390083,0.0008340515778400004,-0.02172667160630226],[1280,720,643.750732421875,366.3055419921875,642.730224609375,641.2587890625,2,-0.05456758290529251,0.06640911102294922,-0.00016342636081390083,0.0008340515778400004,-0.02172667160630226],[1280,800,643.750732421875,406.3055419921875,642.730224609375,641.2587890625,2,-0.05456758290529251,0.06640911102294922,-0.00016342636081390083,0.0008340515778400004,-0.02172667160630226]],"options":[["Backlight Compensation",0.0,0.0,1.0,1.0,0.0,"Enable / disable backlight compensation"],["Brightness",0.0,-64.0,64.0,1.0,0.0,"UVC image brightness"],["Contrast",50.0,0.0,100.0,1.0,50.0,"UVC image contrast"],["Exposure",156.0,1.0,10000.0,1.0,156.0,"Controls exposure time of color camera. Setting any value will disable auto exposure"],["Gain",64.0,0.0,128.0,1.0,64.0,"UVC image gain"],["Gamma",300.0,100.0,500.0,1.0,300.0,"UVC image gamma setting"],["Hue",0.0,-180.0,180.0,1.0,0.0,"UVC image hue"],["Saturation",64.0,0.0,100.0,1.0,64.0,"UVC image saturation setting"],["Sharpness",50.0,0.0,100.0,1.0,50.0,"UVC image sharpness setting"],["White Balance",4600.0,2800.0,6500.0,10.0,4600.0,"Controls white balance of color image. Setting any value will disable auto white balance"],["Enable Auto Exposure",1.0,0.0,1.0,1.0,1.0,"Enable / disable auto-exposure"],["Enable Auto White Balance",1.0,0.0,1.0,1.0,1.0,"Enable / disable auto-white-balance"],["Frames Queue Size",16.0,0.0,32.0,1.0,16.0,"Max number of frames you can hold at a given time. Increasing this number will reduce frame drops but increase latency, and vice versa"],["Power Line Frequency",3.0,0.0,3.0,1.0,3.0,"Power Line Frequency"],["Auto Exposure Priority",0.0,0.0,1.0,1.0,0.0,"Restrict Auto-Exposure to enforce constant FPS rate. Turn ON to remove the restrictions (may result in FPS drop)"],["Global Time Enabled",1.0,0.0,1.0,1.0,1.0,"Enable/Disable global timestamp"]],"recommended-filters":["Decimation Filter"],"stream-name":"Color"}'
    # any removal will still result in too-long a string:
    #     removing intrinsics will yield ~1557
    #     removing options will yield ~1463
    # ideally, we'd have two ellipsis:
    #     {"id":"stream-options","intrinsics":[ ... ],"options":[ ... ],"recommended-filters":["Decimation Filter"],"stream-name":"Color"}
    # but we cannot do that.
    #
    check.equal( short( s,  300 ), '{"id":"stream-options","intrinsics": ... }' )
    check.equal( short( s,  600 ), '{"id":"stream-options","intrinsics": ... }' )
    check.equal( short( s, 1200 ), '{"id":"stream-options","intrinsics": ... }' )
    check.is_false( short( s, 1500 ) == '{"id":"stream-options","intrinsics": ... }' )


#############################################################################################
#
def test_ellipsis_deep_inside():
    s = '{"a[1]":1,"b[2":3,"d":[1,2,{3,4,5,6,7,8,9},6,7,8]}'  # more complicated
    check.equal( short( s ), s )
    check.equal( short( s, len(s) ), s )
    check.equal( short( s, len(s)-1 ), '{"a[1]":1,"b[2":3,"d":[1,2,{3,4,5, ... },6,7,8]}' )
    check.equal( short( s, len(s)-2 ), '{"a[1]":1,"b[2":3,"d":[1,2,{3,4,5, ... },6,7,8]}' )
    check.equal( short( s, len(s)-8 ), '{"a[1]":1,"b[2":3,"d":[1,2,{ ... },6,7,8]}' )
    check.equal( short( s, len(s)-9 ), '{"a[1]":1,"b[2":3,"d":[1,2, ... ]}' )


#############################################################################################
#
def test_none():
    # None translates to json 'null', so is entirely valid
    check.equal( short( None ), 'null' )


#############################################################################################
#
def test_tuples():
    check.equal( short( (1,2,'string') ), '[1,2,"string"]' )
    check.equal( short( (1,2,'string'), 11 ), '[1,2, ... ]' )
    check.equal( short( '(1,2,"string")' ), '(1,2,"string")' )


#############################################################################################
#
def test_json():
    #
    # Just like string input is not enforced as valid json, neither is the output expected to be:
    #
    check.equal( short(
        {'some':'one','two':[1,2, 3, 'dkjsahdkjashdkjhsakjdhada',213123123,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9]} ),
        '{"some":"one","two":[1,2,3," ... ",213123123,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9]}' )
    check.equal( short(
        {'some':'one','two': "1,2, 3, 'dkjsahdkjashdkjhsakjdhada',213123123,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9" } ),
        '{"some":"one","two":"1,2, 3, \'dkjsahdkjashdkjhsakjdhada\',213123123,1,2,3,4,5,6,7,8,9,0,1, ... "}' )
    #
    s = {"a[1]":1,"b[2":3,"d":[1,2,[3,4,5,6,7,8,9],6,7,8]}
    ss = '{"a[1]":1,"b[2":3,"d":[1,2,[3,4,5,6,7,8,9],6,7,8]}'
    check.equal( short( s ), ss )
    check.equal( short( s, len(ss)-1 ), '{"a[1]":1,"b[2":3,"d":[1,2,[3,4,5, ... ],6,7,8]}' )
