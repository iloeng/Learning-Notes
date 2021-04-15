cmake_minimum_required(VERSION 3.15)
project(tinyhttpd_0_1_0)

set(CMAKE_CXX_STANDARD 14)

add_executable(tinyhttpd_0_1_0
        htdocs/check.cgi
        htdocs/color.cgi
        htdocs/index.html
        htdocs/README
        httpd.c
        simpleclient.c)
