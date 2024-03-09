cmake_minimum_required(VERSION 3.22.1)

project("giflib")

add_library(giflib STATIC
        dgif_lib.c egif_lib.c getarg.c
        gif2rgb.c gif_err.c gif_font.c gif_hash.c
        gifalloc.c gifbg.c gifbuild.c gifclrmp.c
        gifcolor.c gifecho.c giffilter.c
        giffix.c gifhisto.c gifinto.c
        gifsponge.c giftext.c giftool.c
        gifwedge.c openbsd-reallocarray.c qprintf.c
        quantize.c
)

target_include_directories(giflib PUBLIC ${CMAKE_SOURCE_DIR}/giflib)
