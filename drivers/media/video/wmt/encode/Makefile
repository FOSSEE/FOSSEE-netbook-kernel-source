#
# Makefile for the video encode device drivers.
#

g_wmt_ve_com-y := wmt-ve.o
g_wmt_ve_h264-y := h264/wmt-h264enc.o h264/header.o h264/vlc.o
g_wmt_ve_jenc-y := jpeg/hw-jenc.o jpeg/wmt-jenc.o

obj-$(CONFIG_WMT_VIDEO_ENCODE) += g_wmt_ve_com.o
obj-$(CONFIG_WMT_VIDEO_ENCODE_H264) += g_wmt_ve_h264.o
obj-$(CONFIG_WMT_VIDEO_ENCODE_H264) += g_wmt_ve_jenc.o
