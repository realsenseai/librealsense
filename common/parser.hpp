// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 RealSense, Inc. All Rights Reserved.

#include <string>
#include <map>
#include <regex>
#include <fstream>
#include <sstream>
#include <vector>
#include <functional>
#include <cstring>
#include <iomanip>

#ifdef ANDROID
#include "android_helpers.h"
#endif

#include "../third-party/rapidxml/rapidxml.hpp"
#include <rsutils/string/string-utilities.h>

#define MAX_PARAMS 4

struct section;
typedef std::function<void(const uint8_t*, const section&, std::stringstream&)> xml_parser_function;

const uint16_t  HW_MONITOR_BUFFER_SIZE = 1024;

#pragma pack(push, 1)
typedef struct
{
    uint8_t     Version_Revision;
    uint8_t     Version_Number;
    uint8_t     Version_Minor;
    uint8_t     Version_Major;
    int         Version_spare;
}ChangeSetVersion;

typedef struct
{
    uint8_t     Version_Minor;
    uint8_t     Version_Major;
    uint8_t     Version_spare[2];
}MajorMinorVersion;

typedef struct
{
    uint8_t     Version1;
    uint8_t     Version2;
    uint8_t     Version3;
    uint8_t     Version4;
}HexByte;

typedef struct
{
    uint8_t     Version2;
    uint8_t     Version1;
    uint8_t     Version_spare[2];
}LiguriaVersion;

typedef struct
{
    uint8_t     sts;
}Bool;

typedef struct
{
    uint8_t     num;
}HwTypeNumber;

typedef struct
{
    uint8_t     Version_Minor;
    uint8_t     Version_Major;
    uint8_t     Version_spare[2];
}WideMajorMinorVersion;

typedef struct
{
    uint8_t     value[32];
}Ascii;

typedef struct
{
    uint8_t     version;
}DecByte;

typedef struct
{
    uint8_t     number1;
    uint8_t     number2;
    uint8_t     number3;
    uint8_t     number4;
}HexNumber;

typedef struct
{
    uint8_t     number1;
    uint8_t     number2;
    uint8_t     number3;
    uint8_t     number4;
}HexNumberReversed;

typedef struct
{
    uint8_t     number1;
    uint8_t     number2;
    uint8_t     number3;
    uint8_t     number4;
    uint8_t     number5;
    uint8_t     number6;
    uint8_t     number7;
    uint8_t     number8;
}BarCodeSerial12Char;

typedef struct
{
    double      number;
}DoubleNumber;
#pragma pack(pop)

struct parameter {
    std::string name, data;
    bool is_decimal, is_reverse_bytes;
    int format_length;

    parameter(std::string in_name, std::string in_data,
        bool in_is_decimal, bool in_is_reverse_bytes,
        int in_format_length)
        : name(in_name),
        format_length(in_format_length),
        is_decimal(in_is_decimal),
        is_reverse_bytes(in_is_reverse_bytes),
        data(in_data)
    {}
    parameter()
        : name(""),
        format_length(2),
        is_decimal(false),
        is_reverse_bytes(false),
        data("")
    {}
};

struct section {
    std::string name = "";
    std::string title = "";
    std::string format_type = "";
    std::string data = "";
    int offset = 0;
    int size = 0;
};

struct data {
    std::vector<section> sections;
};

struct kvp {
    std::string key, value;
};

struct custom_formatter {
    std::string name, key_size;
    std::vector<kvp> kv;
};

typedef struct command
{
    std::string name;
    unsigned int op_code;
    std::string read_format;
    bool is_write_only = false;
    bool is_read_command = false;
    std::string description;
    int time_out = 20000;
    int num_of_parameters = 0;
    bool is_cmd_write_data = false;
    std::string cmd_permission;
    std::string cmd_interface;
    std::string cmd_pipe;
    std::string i2c_reg_offset;
    std::string i2c_cmd_type;

    data read_data;
    std::vector<parameter> parameters;
} command_from_xml;

// units of nibbles
enum FormatSize {
    Byte = 2,
    Word = 4,
    Double = 8
};

struct commands_xml
{
    std::map<std::string, command_from_xml> commands;
    std::map<std::string, custom_formatter> custom_formatters;
};

// While working from files, convert the file content into recognized raw format
inline void file_argument_to_blob(std::vector<std::string>& params)
{
    if (!params.size())
        return;
 
    //The last parameter provided by the user can represent a fully-qualified file path
    std::ifstream file(params.back(), std::ios::binary | std::ios::in);
    if (file.good())
    {
        auto data = std::vector<uint8_t>((std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>());

        size_t unmodified_params = params.size() - 1; // non-negative due to the file parameter
        std::vector<std::string> modified_params(unmodified_params + data.size());
        if (unmodified_params)                        // copy leading parameters
            std::copy(modified_params.begin(), modified_params.begin() + unmodified_params, params.begin());
        std::transform(data.begin(), data.end(), modified_params.begin() + unmodified_params, [](uint8_t c) { return rsutils::string::hexify(c); });
        params = modified_params; // substitute original params with modified data
    }
}

inline void parse_xml_from_memory(const char * content, commands_xml& cmd_xml)
{
    auto doc = std::make_shared<rapidxml::xml_document<>>();
    doc->parse<0>(doc->allocate_string(content));

    auto node = doc->first_node("Commands");
    if (!node)
        throw std::logic_error("Invalid XML file - expecting Commands node to be present");

    for (auto NodeI = node->first_node(); NodeI; NodeI = NodeI->next_sibling())
    {
        if (!strcmp(NodeI->name(), "Command"))
        {
            command_from_xml cmd;
            for (auto AttI = NodeI->first_attribute(); AttI; AttI = AttI->next_attribute())
            {
                std::string value = AttI->value();
                std::string att_name = AttI->name();
                if (att_name == "Name") { cmd.name = value; }
                else if (att_name == "Opcode") { cmd.op_code = rsutils::string::ascii_hex_string_to_uint(value); }
                else if (att_name == "IsWriteOnly") { cmd.is_write_only = rsutils::string::string_to_bool(value); }
                else if (att_name == "ReadFormat") { cmd.read_format = value; }
                else if (att_name == "IsReadCommand") { cmd.is_read_command = rsutils::string::string_to_bool(value); }
                else if (att_name == "CmdPermission") { cmd.cmd_permission = value; }
                else if (att_name == "TimeOut") { cmd.time_out = stoi(value); }
                else if (att_name == "Description") { cmd.description = value; }
                else if (att_name == "CmdInterface") { cmd.cmd_interface = value; }
                else if (att_name == "CmdPipe") { cmd.cmd_pipe = value; }
                else if (att_name == "I2cRegOffset") { cmd.i2c_reg_offset = value; }
                else if (att_name == "I2cCmdType") { cmd.i2c_cmd_type = value; }
            }

            for (auto SubNodeI = NodeI->first_node(); SubNodeI; SubNodeI = SubNodeI->next_sibling()) // Add commands
            {
                std::string str_name = SubNodeI->name();
                if (str_name.find("Parameter") != std::string::npos)
                {
                    parameter param;
                    ++cmd.num_of_parameters;
                    for (auto paramAtt = SubNodeI->first_attribute(); paramAtt; paramAtt = paramAtt->next_attribute())
                    {
                        std::string value = paramAtt->value();
                        std::string att_name = paramAtt->name();
                        if (att_name == "Name") { param.name = value; }
                        else if (att_name == "IsDecimal") { param.is_decimal = rsutils::string::string_to_bool(value); }
                    }
                    cmd.parameters.push_back(param);
                }
                else if (str_name == "Data")
                {
                    parameter param;
                    param.name = "Data";
                    cmd.is_cmd_write_data = true;
                    for (auto dataAtt = SubNodeI->first_attribute(); dataAtt; dataAtt = dataAtt->next_attribute())
                    {
                        std::string value = dataAtt->value();
                        std::string data_att_name = dataAtt->name();

                        if (data_att_name == "Name") { param.name = value; }
                        else if (data_att_name == "IsReverseBytes") { param.is_reverse_bytes = rsutils::string::string_to_bool(value); }
                        else if (data_att_name == "FormatLength") { param.format_length = stoi(value); }
                    }
                    cmd.parameters.push_back(param);
                }
                else if (str_name == "ReadData")
                {
                    for (auto sectionNode = SubNodeI->first_node(); sectionNode; sectionNode = sectionNode->next_sibling()) // Add CMD Parameters
                    {
                        section sec;
                        for (auto sectionAtt = sectionNode->first_attribute(); sectionAtt; sectionAtt = sectionAtt->next_attribute())
                        {
                            std::string value = sectionAtt->value();
                            std::string section_att_name = sectionAtt->name();
                            if (section_att_name == "Name") { sec.name = value; }
                            else if (section_att_name == "Title") { sec.title = value; }
                            else if (section_att_name == "Offset") { sec.offset = stoi(value); }
                            else if (section_att_name == "Size") { sec.size = stoi(value); }
                            else if (section_att_name == "FormatType") { sec.format_type = value; }
                        }
                        cmd.read_data.sections.push_back(sec);
                    }
                }
            }
            cmd_xml.commands.insert(std::pair<std::string, command_from_xml>(rsutils::string::to_lower(cmd.name), cmd));
        }
        else if (!strcmp(NodeI->name(), "CustomFormatter"))
        {
            custom_formatter customFormatter;
            for (auto AttI = NodeI->first_attribute(); AttI; AttI = AttI->next_attribute())
            {
                std::string value = AttI->value();
                std::string atti_name = AttI->name();
                if (atti_name == "KeySize") { customFormatter.key_size = value; }
                else if (atti_name == "Name") { customFormatter.name = value; }
            }

            for (auto SubNodeI = NodeI->first_node(); SubNodeI; SubNodeI = SubNodeI->next_sibling()) // Add Kvp
            {
                std::string sub_nodei_name = SubNodeI->name();
                if (sub_nodei_name.find("KVP") != std::string::npos)
                {
                    kvp kvp;
                    for (auto paramAtt = SubNodeI->first_attribute(); paramAtt; paramAtt = paramAtt->next_attribute())
                    {
                        std::string value = paramAtt->value();
                        std::string param_att_name = paramAtt->name();
                        if (param_att_name == "Key") { kvp.key = value; }
                        else if (param_att_name == "Value") { kvp.value = value; }
                    }
                    customFormatter.kv.push_back(kvp);
                }

            }
            cmd_xml.custom_formatters.insert(std::pair<std::string, custom_formatter>(rsutils::string::to_lower(customFormatter.name), customFormatter));
        }
    }
}

inline bool parse_xml_from_file(const std::string& xml_full_file_path, commands_xml& cmd_xml)
{
    std::ifstream fin(xml_full_file_path);

    if (!fin.good())
        return false;

    std::stringstream ss;
    ss << fin.rdbuf();
    auto xml = ss.str();

    parse_xml_from_memory(xml.c_str(), cmd_xml);

    return true;
}

inline void check_section_size(unsigned section_size, unsigned struct_size, const std::string& section_name, const std::string& struct_name)
{
    std::string exception_str("Size of section %s is bigger than %s struct.");
    if (section_size > struct_size)
    {
        auto msg = "Size of section " + section_name + " is bigger than " + struct_name + " struct!";
        throw std::runtime_error(msg);
    }
}

inline void update_format_type_to_lambda(std::map<std::string, xml_parser_function>& format_type_to_lambda)
{
    format_type_to_lambda.insert(std::make_pair("ChangeSetVersion", [](const uint8_t* data_offset, const section& sec, std::stringstream& tempStr) {
        check_section_size(sec.size, sizeof(ChangeSetVersion), sec.name.c_str(), "ChangeSetVersion");
        auto changeSetVersion = reinterpret_cast<const ChangeSetVersion*>(data_offset + sec.offset);
        tempStr << static_cast<int>(changeSetVersion->Version_Major) <<
            ((sec.size >= 2) ? ("." + std::to_string(static_cast<int>(changeSetVersion->Version_Minor))) : "") <<
            ((sec.size >= 3) ? ("." + std::to_string(static_cast<int>(changeSetVersion->Version_Number))) : "") <<
            ((sec.size >= 4) ? ("." + std::to_string(static_cast<int>(changeSetVersion->Version_Revision))) : "") <<
            ((sec.size >= 5) ? (" (" + std::to_string(changeSetVersion->Version_spare)) + ")" : "");
    }));

    format_type_to_lambda.insert(std::make_pair("MajorMinorVersion", [&](const uint8_t* data_offset, const section& sec, std::stringstream& tempStr) {
        check_section_size(sec.size, sizeof(MajorMinorVersion), sec.name.c_str(), "MajorMinorVersion");
        auto majorMinorVersion = reinterpret_cast<const MajorMinorVersion*>(data_offset + sec.offset);;
        tempStr << static_cast<int>(majorMinorVersion->Version_Major) <<
            ((sec.size >= 2) ? ("." + std::to_string(static_cast<int>(majorMinorVersion->Version_Minor))) : "");
    }));

    format_type_to_lambda.insert(std::make_pair("HexByte", [&](const uint8_t* data_offset, const section& sec, std::stringstream& tempStr) {
        check_section_size(sec.size, sizeof(HexByte), sec.name.c_str(), "HexByte");
        auto hexByte = reinterpret_cast<const HexByte*>(data_offset + sec.offset);
        tempStr << rsutils::string::hexify(hexByte->Version4);
    }));

    format_type_to_lambda.insert(std::make_pair("LiguriaVersion", [&](const uint8_t* data_offset, const section& sec, std::stringstream& tempStr) {
        check_section_size(sec.size, sizeof(LiguriaVersion), sec.name.c_str(), "LiguriaVersion");
        auto liguriaVersion = reinterpret_cast<const LiguriaVersion*>(data_offset + sec.offset);
        tempStr << static_cast<int>(liguriaVersion->Version1) <<
            ((sec.size >= 2) ? ("." + std::to_string(static_cast<int>(liguriaVersion->Version2))) : "");
    }));

    format_type_to_lambda.insert(std::make_pair("Bool", [&](const uint8_t* data_offset, const section& sec, std::stringstream& tempStr) {
        check_section_size(sec.size, sizeof(Bool), sec.name.c_str(), "Bool");
        auto BooL = reinterpret_cast<const Bool*>(data_offset + sec.offset);
        tempStr << ((static_cast<int>(BooL->sts)) ? "TRUE" : "FALSE");
    }));

    format_type_to_lambda.insert(std::make_pair("HwTypeNumber", [&](const uint8_t* data_offset, const section& sec, std::stringstream& tempStr) {
        check_section_size(sec.size, sizeof(HwTypeNumber), sec.name.c_str(), "HwTypeNumber");
        auto hwTypeNumber = reinterpret_cast<const HwTypeNumber*>(data_offset + sec.offset);
        tempStr << static_cast<int>(hwTypeNumber->num);
    }));

    format_type_to_lambda.insert(std::make_pair("Ascii", [&](const uint8_t* data_offset, const section& sec, std::stringstream& tempStr) {
        check_section_size(sec.size, sizeof(Ascii), sec.name.c_str(), "Ascii");
        auto ascii = reinterpret_cast<const Ascii*>(data_offset + sec.offset);
        auto temp = new char[sec.size + 1];
        memcpy(temp, ascii->value, sec.size);
        temp[sec.size] = '\0';
        tempStr << temp;
        delete[] temp;
    }));

    format_type_to_lambda.insert(std::make_pair("DecByte", [&](const uint8_t* data_offset, const section& sec, std::stringstream& tempStr) {
        check_section_size(sec.size, sizeof(DecByte), sec.name.c_str(), "DecByte");
        auto decByte = reinterpret_cast<const DecByte*>(data_offset + sec.offset);
        tempStr << static_cast<int>(decByte->version);
    }));

    format_type_to_lambda.insert(std::make_pair("Integer", [&](const uint8_t* data_offset, const section& sec, std::stringstream& tempStr) {
        auto read_integer = [&](auto dummy) {
            using T = decltype(dummy);
            check_section_size(sec.size, sizeof(T), sec.name.c_str(), "Integer");
            T val;
            memcpy(&val, data_offset + sec.offset, sizeof(T));
            return val;
        };

        switch (sec.size) {
            case 1: tempStr << static_cast<int>(read_integer(uint8_t{})); break;
            case 2: tempStr << read_integer(uint16_t{}); break;
            case 4: tempStr << read_integer(uint32_t{}); break;
            case 8: tempStr << read_integer(uint64_t{}); break;
            default: throw std::runtime_error("Unsupported Integer size: " + std::to_string(sec.size) + " for section: " + sec.name);
        }
    }));

    format_type_to_lambda.insert(std::make_pair("HexNumber", [&](const uint8_t* data_offset, const section& sec, std::stringstream& tempStr) {
        check_section_size(sec.size, sizeof(HexNumber), sec.name.c_str(), "HexNumber");
        auto hexNumber = reinterpret_cast<const HexNumber*>(data_offset + sec.offset);
        tempStr << rsutils::string::hexify(hexNumber->number1) <<
            ((sec.size >= 2) ? rsutils::string::hexify(hexNumber->number2) : "") <<
            ((sec.size >= 3) ? rsutils::string::hexify(hexNumber->number3) : "") <<
            ((sec.size >= 4) ? rsutils::string::hexify(hexNumber->number4) : "");
    }));

    format_type_to_lambda.insert(std::make_pair("HexNumberTwoBytes", [&](const uint8_t* data_offset, const section& sec, std::stringstream& tempStr) {
        check_section_size(sec.size, sizeof(HexNumber), sec.name.c_str(), "HexNumber");
        auto hexNumber = reinterpret_cast<const HexNumber*>(data_offset + sec.offset);
        tempStr << rsutils::string::hexify(hexNumber->number2) <<
            ((sec.size >= 2) ? rsutils::string::hexify(hexNumber->number1) : "");
    }));

    format_type_to_lambda.insert(std::make_pair("HexNumberReversed", [&](const uint8_t* data_offset, const section& sec, std::stringstream& tempStr) {
        check_section_size(sec.size, sizeof(HexNumberReversed), sec.name.c_str(), "HexNumberReversed");
        auto hexNumberReversed = reinterpret_cast<const HexNumberReversed*>(data_offset + sec.offset);
        tempStr << rsutils::string::hexify(hexNumberReversed->number4) <<
            ((sec.size >= 2) ? rsutils::string::hexify(hexNumberReversed->number3) : "") <<
            ((sec.size >= 3) ? rsutils::string::hexify(hexNumberReversed->number2) : "") <<
            ((sec.size >= 4) ? rsutils::string::hexify(hexNumberReversed->number1) : "");
    }));

    format_type_to_lambda.insert(std::make_pair("BarCodeSerial12Char", [&](const uint8_t* data_offset, const section& sec, std::stringstream& tempStr) {
        check_section_size(sec.size, sizeof(BarCodeSerial12Char), sec.name.c_str(), "BarCodeSerial12Char");
        auto barCodeSerial12Char = reinterpret_cast<const BarCodeSerial12Char*>(data_offset + sec.offset);
        tempStr << rsutils::string::hexify(barCodeSerial12Char->number1) <<
            ((sec.size >= 2) ? rsutils::string::hexify(barCodeSerial12Char->number2) : "") <<
            ((sec.size >= 3) ? rsutils::string::hexify(barCodeSerial12Char->number3) : "") <<
            ((sec.size >= 4) ? rsutils::string::hexify(barCodeSerial12Char->number4) : "") <<
            ((sec.size >= 5) ? rsutils::string::hexify(barCodeSerial12Char->number5) : "") <<
            ((sec.size >= 6) ? rsutils::string::hexify(barCodeSerial12Char->number6) : "") <<
            ((sec.size >= 7) ? rsutils::string::hexify(barCodeSerial12Char->number7) : "") <<
            ((sec.size >= 8) ? rsutils::string::hexify(barCodeSerial12Char->number8) : "");
    }));

    format_type_to_lambda.insert(std::make_pair("WideMajorMinorVersion", [&](const uint8_t* data_offset, const section& sec, std::stringstream& tempStr) {
        check_section_size(sec.size, sizeof(WideMajorMinorVersion), sec.name.c_str(), "WideMajorMinorVersion");
        auto wideMajorMinorVersion = reinterpret_cast<const WideMajorMinorVersion*>(data_offset + sec.offset);
        tempStr << static_cast<int>(wideMajorMinorVersion->Version_Minor) <<
            ((sec.size >= 2) ? ("." + std::to_string(static_cast<int>(wideMajorMinorVersion->Version_Major))) : "");
    }));

    format_type_to_lambda.insert(std::make_pair("Double", [&](const uint8_t* data_offset, const section& sec, std::stringstream& tempStr) {
        check_section_size(sec.size, sizeof(DoubleNumber), sec.name.c_str(), "DoubleNumber");
        auto number = reinterpret_cast<const DoubleNumber*>(data_offset + sec.offset);
        tempStr << std::setprecision(10) << number->number;
    }));
}

inline void update_sections_data(const uint8_t* data_offset, std::vector<section>& sections, const std::map<std::string, custom_formatter>& custom_formatters, const std::map<std::string, xml_parser_function>& format_type_to_lambda)
{
    for (auto& sec : sections)
    {
        std::stringstream tempStr;
        auto pair = format_type_to_lambda.find(sec.format_type);
        if (pair != format_type_to_lambda.end())
        {
            pair->second(data_offset, sec, tempStr);
        }
        else
        {
            // FormatType not found. searching in the custom format list
            auto it = custom_formatters.find(rsutils::string::to_lower(sec.format_type));
            if (it != custom_formatters.end())
            {
                if (it->second.key_size == "Byte")
                {
                    auto key = uint8_t(*(data_offset + sec.offset));
                    for (auto& elem : it->second.kv)
                    {
                        auto keyStr = std::to_string(int(key));
                        if (elem.key == keyStr)
                        {
                            tempStr << elem.value;
                            break;
                        }
                    }
                }
            }
        }

        std::vector<uint8_t> raw_data;
        if (sec.size == 0)
            throw std::runtime_error(std::string("Size of section " + sec.name + " is 0!").c_str());

        raw_data.resize(sec.size);
        memcpy(raw_data.data(), data_offset + sec.offset, sec.size);
        sec.data = tempStr.str();
    }
}

inline void decode_string_from_raw_data(const command_from_xml& command, const std::map<std::string, custom_formatter>& custom_formatters, const uint8_t* raw_data_offset, size_t data_size, std::string& output, const std::map<std::string, xml_parser_function>& format_type_to_lambda)
{
    auto data_offset = raw_data_offset + 4;
    data_size -= 4;
    if (data_size == 0)
        throw std::runtime_error("Raw data is empty!");

    std::stringstream ss_output;
    ss_output.str("");
    auto num_of_bytes_for_space = 1;
    auto num_of_bytes_for_new_line = 16;
    auto read_format = command.read_format;
    if (read_format == "Bytes")
        num_of_bytes_for_space = 1;
    else if (read_format == "Words")
        num_of_bytes_for_space = 2;
    else if (read_format == "Doubles")
        num_of_bytes_for_space = 4;


    if (command.read_data.sections.empty())
    {
        std::vector<std::string> buffer;
        auto bytes = 0;
        auto bytes_per_line = 0;
        auto start_address = "0";
        std::string curr_address = start_address;
        std::stringstream dec_to_hex;
        auto end_address = std::stoi(start_address) + data_size;
        auto num_of_zeros = 1;

        if (curr_address.size() == 1)
        {
            auto temp_curr_address = curr_address;
            auto temp_end_address = end_address;
            auto temp_bytes_per_line = bytes_per_line;

            for (size_t offset = 0; offset < data_size; ++offset)
            {
                ++temp_bytes_per_line;
                if (temp_bytes_per_line == num_of_bytes_for_new_line)
                {

                    unsigned int next_add = stoi(temp_curr_address) + num_of_bytes_for_new_line;

                    if (next_add >= temp_end_address)
                        break;

                    dec_to_hex.str("");
                    dec_to_hex << std::hex << next_add;
                    temp_curr_address = dec_to_hex.str();

                    if (temp_curr_address.size() == 1)
                        temp_curr_address.insert(0, "0");

                    temp_curr_address = std::to_string(next_add);
                    temp_bytes_per_line = 0;
                }
            }

            num_of_zeros = int(dec_to_hex.str().length());
            ss_output << "Offset: 0x";
            dec_to_hex.str("");

            for (auto i = 1; i < num_of_zeros; ++i)
            {
                ss_output << "0";
            }
        }
        else
            ss_output << "Offset: 0x";

        ss_output << std::hex << stoi(curr_address) << " => ";

        for (size_t offset = 0; offset < data_size; ++offset)
        {
            ++bytes_per_line;
            ++bytes;
            buffer.push_back(rsutils::string::hexify(static_cast<unsigned char>(*(data_offset + offset))));

            if (bytes == num_of_bytes_for_space)
            {
                bytes = 0;
                reverse(buffer.begin(), buffer.end());

                for (auto str : buffer)
                    ss_output << str;

                buffer.clear();

                if (bytes_per_line == num_of_bytes_for_new_line)
                {
                    unsigned int next_add = stoi(curr_address) + num_of_bytes_for_new_line;

                    if (next_add >= end_address)
                        break;

                    dec_to_hex.str("");
                    dec_to_hex << std::hex << next_add;
                    curr_address = dec_to_hex.str();

                    auto putZeros = num_of_zeros - int(curr_address.size());
                    ss_output << "\nOffset: 0x";

                    for (auto i = 0; i < putZeros; ++i)
                    {
                        ss_output << "0";
                    }

                    ss_output << curr_address << " => ";
                    curr_address = std::to_string(next_add);
                    bytes_per_line = 0;
                }
                else
                {
                    if ((offset + 1) < data_size)
                        ss_output << " ";
                }
            }
        }
    }
    else
    {
        auto sections = command.read_data.sections;
        update_sections_data(data_offset, sections, custom_formatters, format_type_to_lambda);
        unsigned max_line_len = 0;
        for (auto& elem : sections)
            max_line_len = ((elem.name.size() > max_line_len) ? unsigned(elem.name.size()) : max_line_len);

        const int spaces = 3; // number of spaces between section name to section data
        for (auto& elem : sections)
            ss_output << elem.name << ":" << std::setw((max_line_len + spaces) - elem.name.size() + elem.data.length()) << std::right << elem.data << std::endl;
    }

    output = ss_output.str();
}

inline void encode_raw_data_command(const command_from_xml& xml_cmd_info, const std::vector<parameter>& params, std::vector<uint8_t>& raw_data)
{
    auto cmd_op_code = xml_cmd_info.op_code;
    auto is_cmd_writes_data = xml_cmd_info.is_cmd_write_data;
    auto parameters = params;

    auto num_of_required_parameters = xml_cmd_info.num_of_parameters + ((xml_cmd_info.is_cmd_write_data) ? 1 : 0);
    if (int(params.size()) < num_of_required_parameters)
        throw std::runtime_error("Number of given parameters is less than required!"); // TODO: consider to print the command info

    auto format_length = Byte;
    if (is_cmd_writes_data)
        format_length = ((params[xml_cmd_info.num_of_parameters].format_length == -1) ? format_length : static_cast<FormatSize>(params[xml_cmd_info.num_of_parameters].format_length));

    const uint16_t pre_header_data = 0xcdab;  // magic number
    raw_data.resize(HW_MONITOR_BUFFER_SIZE); // TODO: resize std::vector to the right size
    auto write_ptr = raw_data.data();
    auto header_size = 4;


    auto cur_index = 2;
    *reinterpret_cast<uint16_t *>(write_ptr + cur_index) = pre_header_data;
    cur_index += sizeof(uint16_t);
    *reinterpret_cast<unsigned int *>(write_ptr + cur_index) = cmd_op_code;
    cur_index += sizeof(unsigned int);

    // Parameters
    for (auto param_index = 0; param_index < MAX_PARAMS; ++param_index)
    {
        if (param_index < xml_cmd_info.num_of_parameters)
        {
            unsigned decimal;
            std::stringstream ss;
            ss << params[param_index].data;
            ss >> std::hex >> decimal;
            *reinterpret_cast<unsigned*>(write_ptr + cur_index) = decimal;
            cur_index += sizeof(unsigned);
        }
        else
        {
            *reinterpret_cast<unsigned *>(write_ptr + cur_index) = 0;
            cur_index += sizeof(unsigned);
        }
    }

    // Data
    if (is_cmd_writes_data)
    {
        for (auto j = xml_cmd_info.num_of_parameters; j < int(params.size()); ++j)
        {
            unsigned decimal;
            std::stringstream ss;
            ss << params[j].data;
            ss >> std::hex >> decimal;
            switch (format_length)
            {
            case Byte:
                *reinterpret_cast<uint8_t*>(write_ptr + cur_index) = static_cast<uint8_t>(decimal);
                cur_index += sizeof(uint8_t);
                break;

            case Word:
                *reinterpret_cast<short *>(write_ptr + cur_index) = static_cast<short>(decimal);
                cur_index += sizeof(short);
                break;

            case Double:
                *reinterpret_cast<unsigned *>(write_ptr + cur_index) = static_cast<unsigned>(decimal);
                cur_index += sizeof(unsigned);
                break;

            default:
                *reinterpret_cast<uint8_t*>(write_ptr + cur_index) = static_cast<uint8_t>(decimal);
                cur_index += sizeof(uint8_t);
                break;
            }
        }
    }

    *reinterpret_cast<uint16_t*>(raw_data.data()) = static_cast<uint16_t>(cur_index - header_size);// Length doesn't include hdr.
    raw_data.resize(cur_index); // TODO:
}

