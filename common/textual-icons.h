// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.
#pragma once


namespace rs2
{
    struct textual_icon
    {
        template<typename CharT, std::size_t N>
        explicit constexpr textual_icon(const CharT(&unicode_icon)[N])
            : _icon{ * reinterpret_cast<const char*>(&unicode_icon[0])
                   , * reinterpret_cast<const char*>(&unicode_icon[1])
                   , * reinterpret_cast<const char*>(&unicode_icon[2])
                   , * reinterpret_cast<const char*>(&unicode_icon[3]), '\0' }
        {
        }

        operator const char* () const
        {
            return _icon.data();
        }
    private:
        std::array<char, 5> _icon;
    };

    inline std::ostream& operator<<(std::ostream& os, const textual_icon& i)
    {
        return os << static_cast<const char*>(i);
    }

    namespace textual_icons
    {
        // A note to a maintainer - preserve order when adding values to avoid duplicates
        static const textual_icon search{ u8"\uf002" };
        static const textual_icon file_movie{ u8"\uf008" };
        static const textual_icon check{ u8"\uf00c" };
        static const textual_icon times{ u8"\uf00d" };
        static const textual_icon power_off{ u8"\uf011" };
        static const textual_icon cog{ u8"\uf013" };
        static const textual_icon download{ u8"\uf019" };
        static const textual_icon envelope{ u8"\uf01c" };
        static const textual_icon rotate{ u8"\uf01e" };
        static const textual_icon refresh{ u8"\uf021" };
        static const textual_icon lock{ u8"\uf023" };
        static const textual_icon camera{ u8"\uf030" };
        static const textual_icon video_camera{ u8"\uf03d" };
        static const textual_icon tint{ u8"\uf043" };
        static const textual_icon edit{ u8"\uf044" };
        static const textual_icon step_backward{ u8"\uf048" };
        static const textual_icon play{ u8"\uf04b" };
        static const textual_icon pause{ u8"\uf04c" };
        static const textual_icon stop{ u8"\uf04d" };
        static const textual_icon step_forward{ u8"\uf051" };
        static const textual_icon plus_circle{ u8"\uf055" };
        static const textual_icon times_circle{ u8"\uf057" };
        static const textual_icon question_mark{ u8"\uf059" };
        static const textual_icon info_circle{ u8"\uf05a" };
        static const textual_icon arrow_left{ u8"\uf060" };
        static const textual_icon arrow_right{ u8"\uf061" };
        static const textual_icon arrow_up{ u8"\uf062" };
        static const textual_icon arrow_down{ u8"\uf063" };
        static const textual_icon compress{ u8"\uf066" };
        static const textual_icon minus{ u8"\uf068" };
        static const textual_icon eye_slash{ u8"\uf070" };
        static const textual_icon exclamation_triangle{ u8"\uf071" };
        static const textual_icon chevron_down{ u8"\uf078" };
        static const textual_icon shopping_cart{ u8"\uf07a" };
        static const textual_icon folder_open{ u8"\uf07c" };
        static const textual_icon bar_chart{ u8"\uf080" };
        static const textual_icon external_link{ u8"\uf08e" };
        static const textual_icon trophy{ u8"\uF091" };
        static const textual_icon upload{ u8"\uf093" };
        static const textual_icon square_o{ u8"\uf096" };
        static const textual_icon unlock{ u8"\uf09c" };
        static const textual_icon list_ul{ u8"\uf0ae" };
        static const textual_icon up_down_left_right{ u8"\uf0b2" };
        static const textual_icon save{ u8"\uf0c7" };
        static const textual_icon check_square{ u8"\uf0c8" };
        static const textual_icon bars{ u8"\uf0c9" };
        static const textual_icon sort_asc{ u8"\uf0d0" };
        static const textual_icon caret_down{ u8"\uf0d7" };
        static const textual_icon repeat{ u8"\uf0e2" };
        static const textual_icon angle_double_up{ u8"\uf102" };
        static const textual_icon angle_double_down{ u8"\uf103" };
        static const textual_icon angle_right{ u8"\uf106" };
        static const textual_icon angle_left{ u8"\uf107" };
        static const textual_icon circle{ u8"\uf111" };
        static const textual_icon minus_square_o{ u8"\uf120" };
        static const textual_icon circle_chevron_left{ u8"\uf137" };
        static const textual_icon circle_chevron_right{ u8"\uf138" };
        static const textual_icon circle_chevron_up{ u8"\uf139" };
        static const textual_icon circle_chevron_down{ u8"\uf13a" };
        static const textual_icon ellipsis_h{ u8"\uf141" };
        static const textual_icon check_square_o{ u8"\uf14a" };
        static const textual_icon cube{ u8"\uf1b2" };
        static const textual_icon cubes{ u8"\uf1b3" };
        static const textual_icon codepen{ u8"\uf1cb" };
        static const textual_icon wifi{ u8"\uf1eb" };
        static const textual_icon toggle_off{ u8"\uf204" };
        static const textual_icon toggle_on{ u8"\uf205" };
        static const textual_icon connectdevelop{ u8"\uf20e" };
        static const textual_icon hourglass{ u8"\uf251" };
        static const textual_icon industry{ u8"\uf275" };
        static const textual_icon usb{ u8"\uf287" };
        static const textual_icon braille{ u8"\uf2a1" };
        static const textual_icon window_maximize{ u8"\uf2d0" };
        static const textual_icon window_restore{ u8"\uf2d2" };
        static const textual_icon microchip{ u8"\uf2db" };
        static const textual_icon ruler{ u8"\uf545" };
        static const textual_icon palette{ u8"\uf576" };
        static const textual_icon adjust{ u8"\uf5aa" };
        static const textual_icon grid_6{ u8"\uf58d" };
        static const textual_icon draw_polygon{ u8"\uf5ee" };
    }

}
