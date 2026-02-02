import sys

def add_style(option, val):
    # colorize ON and OFF keywords
    val = val.replace('ON', '<span style="color: #4CAF50;">ON</span>')
    val = val.replace('OFF', '<span style="color: #E74C3C;">OFF</span>')

    # if more complicated conditions are being used, this can be disabled -
    # if we see an option more than once at add_row, just set its value to be 'Environment dependant'
    # this should handle if/elseif/else structures of cmake
    if in_cond:
        # text will say Environment dependant, and if hovered will show the condition
        val = f'<span class="tooltip">Environment dependant<span class="tooltip-text">{val}<br/></span></span>'
    elif in_elseif:
        # append condition to tooltip
        val = table_rows[option][2].replace("<br/></span></span>", f',<br/> {val}<br/></span></span>')

    elif in_else:
        # append otherwise to tooltip
        val = table_rows[option][2].replace("<br/></span></span>", f',<br/>{val} otherwise</span></span>')

    return val

def add_row(option, description, value):
    advanced = "YES" if option in advanced_options else "NO"
    if option in table_rows:
        # update value if option exists
        table_rows[option][2] = value
        table_rows[option][3] = advanced
    else:
        # add to table new option
        table_rows[option] = [option, description, value, advanced]

def update_advanced_status():
    for option in table_rows:
        if option in advanced_options:
            table_rows[option][3] = "YES"  # Update advanced status to YES

with open('CMake/lrs_options.cmake', 'r') as file:
    lines = file.readlines()

table_rows = {}
advanced_options = set()
in_cond = False
in_elseif = False
in_else = False
current_condition = None
current_comment = ""
last_option = None  # Track the last option processed

for i, line in enumerate(lines):
    if line.strip().startswith(('option(', 'set(')):
        line = ' '.join(line.split())  # remove consecutive spaces
        parts = line.strip().split(' ')
        if line.strip().startswith('option('):
            option = parts[0].strip('option(')
            value = parts[-1].strip(")")  # the last word in line is the value (ON/OFF by default)

            # concatenate rest of the line - expected to be just the description in quotes
            description = ' '.join(parts[1:-1]).strip('"')
        else:
            option = parts[0].strip('set(')
            value = parts[1]
            value = f'<code><span style="color: #2fcfc9;">{value}</span></code>'
            # parts[2] is expected to be 'CACHE'
            vartype = parts[3]  # INT, STRING ...

            # concatenate rest of the line - expected to be just the description in quotes
            description = ' '.join(parts[4:-1]).strip('"') + f" (type {vartype})"

        if current_comment:
            description += " <span style='font-style: italic; color: #666;'>" + current_comment + "</span>"
            current_comment = ""

        if in_cond or in_elseif:
            value = f'{value} if <b><code>{current_condition}</code></b>'

        value = add_style(option, value)
        add_row(option, description, value)
        last_option = option  # Update the last option processed
    elif line.startswith('if'):
        parts = line.strip().split('(', 1)
        condition = parts[1][:-1]  # remove last ")" - part of the 'if'
        current_condition = condition
        in_cond = True
    elif line.startswith('elseif'):
        parts = line.strip().split('(', 1)
        condition = parts[1][:-1]  # remove last ")" - part of the 'if'
        current_condition = condition
        in_cond = False
        in_elseif = True
    elif line.startswith('else'):
        in_cond = False
        in_elseif = False
        in_else = True
    elif line.startswith('endif'):
        current_condition = None
        in_cond = False
        in_else = False
    elif line.startswith('##'):
        continue  # ignore internal comments
    elif line.startswith('#'):
        current_comment += line.strip('# \n')
    elif line.startswith('mark_as_advanced'):
        parts = line.strip().split('(')
        option = parts[1].strip(')')  # extract the option name
        advanced_options.add(option)  # add to advanced options set
        if last_option:  # Check if last_option is set
            advanced_options.add(last_option)  # Mark the last option as advanced
    elif line.strip():
        # if we reach a line that doesn't match the pattern, throw an error as it's not handled (shouldn't happen)
        raise Exception(f"{i, line} not handled")

# Update advanced status for all options after processing the file
update_advanced_status()

def format_dict_values():
    return ''.join(
                    f'\n      <tr>'
                    f'\n\t<td>\n\t  <b><code>{option}</code></b>'f'\n\t</td>'
                    f'\n\t<td>\n\t  {description}\n\t</td>'
                    f'\n\t<td>\n\t  {value}\n\t</td>'
                    f'\n\t<td>\n\t  <span style="color: {"#4CAF50" if advanced == "YES" else "#E74C3C"};">{advanced}</span>\n\t</td>'
                    f'\n      </tr>'
                    for option, description, value, advanced in table_rows.values())

def get_sdk_version():
    if len(sys.argv) > 1:
        # the script can get the version number as a parameter
        version = sys.argv[1]
        return f'Version {version}'
    else:
        # if no parameter provided - no version number will be included
        return ""

html = f'''<!DOCTYPE html>
<html>
<head>
  <title>Build Customization Flags</title>
  <link rel="icon" href="build-flags.ico" type="image/icon type">
  <link rel="stylesheet" href="build-flags.css">
</head>
<body>
  <div class="container">
    <h1>RealSense SDK Build Customization Flags</h1>
    <h2>{get_sdk_version()}</h2>
    <table>
      <tr>
        <th style="width: 25%">Option</th>
        <th style="width: 45%">Description</th>
        <th style="width: 15%">Default</th>
        <th style="width: 15%">Advanced</th>
      </tr>{format_dict_values()}
    </table>
  </div>
</body>
</html>
'''

with open('doc/build-flags.html', 'w') as file:
    file.write(html)
    print("build-flags.html generated")
