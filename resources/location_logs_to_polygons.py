import os
import pathlib
import re
import socket
from win32comext.shell import shell, shellcon


def convert_log_to_points(content: str):
    text = re.sub(r'Time=[^ ]+ X=([^ ]+) Y=(.+)', 'point[idx].x = \\1\npoint[idx].y = \\2', content, 0, re.MULTILINE)
    count = 0
    while text.find('[idx]') != -1:
        text = text.replace('[idx]', f'[{count//2}]', 1)
        count += 1
    return text


def convert_points_to_polygon(polygon_name: str, content: str):
    count = 0
    num = str(count).zfill(3)
    name = f'[custompolygon{num}]\n'
    append = f'''color = 0xC8FFFFFF
color_sub = 0x0
name = {polygon_name}
map = 0
visible = true
draw_on_terrain = true
filled = false
    '''
    return name + content + append


computer_name = socket.gethostname()
documents_folder = shell.SHGetFolderPath(0, shellcon.CSIDL_PERSONAL, None, 0)

log_folder = os.path.join(documents_folder, "GWToolboxpp", computer_name, "location logs")
for filename in os.listdir(log_folder):
    filepath = os.path.join(log_folder, filename)
    filepath = pathlib.Path(filepath)
    if filepath.suffix != ".log":
        continue
    txt_path = filepath.with_suffix(".txt")
    if txt_path.exists():
        continue
    log_text = filepath.read_text()
    txt_text = convert_log_to_points(log_text)
    polygon_name = pathlib.Path(filename).stem
    polygon_name_arr = polygon_name.split(" - ")
    polygon_name_final = polygon_name_arr[0] + ' - ' + polygon_name_arr[-2] + ' - ' + polygon_name_arr[-1]
    polygon_text = convert_points_to_polygon(polygon_name_final, txt_text)
    txt_path.write_text(polygon_text)
