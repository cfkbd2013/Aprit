QT += core gui widgets

CONFIG += c++17 release

# 源文件必须和当前目录的文件名完全一致
SOURCES += aprit_1.0.0.cpp

# 输出可执行文件
TARGET = aprit
DESTDIR = ./

# 编译临时文件目录（自动创建）
MOC_DIR = ./moc
OBJECTS_DIR = ./obj
