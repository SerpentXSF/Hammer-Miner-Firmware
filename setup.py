import os
import shutil
import re
from datetime import datetime

current_directory = os.getcwd()
sdkconfig_path = os.path.join(current_directory, "sdkconfig")
output_path = os.path.join(current_directory, "build")
idf_component_path = os.path.join(current_directory, "main")
idf_component_path = os.path.join(idf_component_path, "idf_component.yml")

# os.system("git submodule init")
# os.system("git submodule update")

if os.path.isfile(idf_component_path):
    os.remove(idf_component_path)
if os.path.isfile(sdkconfig_path):
    print("Delete temporary files")
    os.remove(sdkconfig_path)
if os.path.exists(output_path):
    print("Delete temporary files")
    shutil.rmtree(output_path)

r = os.system("idf.py > " + os.devnull)
if r != 0:
    print(
        "Unable to execute idf.py, please see here to learn how to use and install https://github.com/espressif/esp-idf"
    )
    exit()


def copy_file(source_path, destination_path):
    try:
        shutil.copy(source_path, destination_path)
        print(f"Set default compilation configuration")
    except FileNotFoundError:
        print("File not found, please check the file path.")
    except PermissionError:
        print("No permission to access file, please check file permissions.")
    except Exception as e:
        print(f"An error occurred: {e}")


def update_sdkconfig_version(file_path):
    current_time = datetime.now().strftime("%Y-%m-%d-%H-%M-%S CST")
    
    with open(file_path, 'r', encoding='utf-8') as file:
        content = file.read()
    
    # 使用正则表达式替换 CONFIG_APP_PROJECT_VER 的值
    # 匹配模式：CONFIG_APP_PROJECT_VER="任意内容"
    pattern = r'CONFIG_APP_PROJECT_VER="[^"]*"'
    replacement = f'CONFIG_APP_PROJECT_VER="{current_time}"'
    new_content = re.sub(pattern, replacement, content)
    
    if new_content != content:
        with open(file_path, 'w', encoding='utf-8') as file:
            file.write(new_content)
        print(f"update CONFIG_APP_PROJECT_VER : {current_time}")
    else:
        # 如果没有找到匹配项，可能需要添加该配置
        print("could not find CONFIG_APP_PROJECT_VER in sdkconfig")


def build_frontend():
    os.chdir("main/http_server/axe-os")
    os.system("npm i")
    os.system("npm run build")
    os.chdir("../../..")
    print("***********Frontend building done.***********")


def perform_action():
    os.system("idf.py set-target esp32s3")
    copy_file(
        os.path.join(current_directory, "sdkconfig.hammer"),
        "sdkconfig",
    )

    update_sdkconfig_version(sdkconfig_path)
    build_frontend()
    os.system("idf.py reconfigure")
    os.system("idf.py clean build")


if __name__ == "__main__":

    perform_action()
