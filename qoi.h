#ifndef QOI_FORMAT_CODEC_QOI_H_
#define QOI_FORMAT_CODEC_QOI_H_

#include "utils.h"

constexpr uint8_t QOI_OP_INDEX_TAG = 0x00;
constexpr uint8_t QOI_OP_DIFF_TAG = 0x40;
constexpr uint8_t QOI_OP_LUMA_TAG = 0x80;
constexpr uint8_t QOI_OP_RUN_TAG = 0xc0;
constexpr uint8_t QOI_OP_RGB_TAG = 0xfe;
constexpr uint8_t QOI_OP_RGBA_TAG = 0xff;
constexpr uint8_t QOI_PADDING[8] = {0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u};
constexpr uint8_t QOI_MASK_2 = 0xc0;

/**
 * @brief encode the raw pixel data of an image to qoi format.
 *
 * @param[in] width image width in pixels
 * @param[in] height image height in pixels
 * @param[in] channels number of color channels, 3 = RGB, 4 = RGBA
 * @param[in] colorspace image color space, 0 = sRGB with linear alpha, 1 = all channels linear
 *
 * @return bool true if it is a valid qoi format image, false otherwise
 */
bool QoiEncode(uint32_t width, uint32_t height, uint8_t channels, uint8_t colorspace = 0);

/**
 * @brief decode the qoi format of an image to raw pixel data
 *
 * @param[out] width image width in pixels
 * @param[out] height image height in pixels
 * @param[out] channels number of color channels, 3 = RGB, 4 = RGBA
 * @param[out] colorspace image color space, 0 = sRGB with linear alpha, 1 = all channels linear
 *
 * @return bool true if it is a valid qoi format image, false otherwise
 */
bool QoiDecode(uint32_t &width, uint32_t &height, uint8_t &channels, uint8_t &colorspace);


bool QoiEncode(uint32_t width, uint32_t height, uint8_t channels, uint8_t colorspace) {

    // qoi-header part

    // write magic bytes "qoif"
    QoiWriteChar('q');
    QoiWriteChar('o');
    QoiWriteChar('i');
    QoiWriteChar('f');
    // write image width
    QoiWriteU32(width);
    // write image height
    QoiWriteU32(height);
    // write channel number
    QoiWriteU8(channels);
    // write color space specifier
    QoiWriteU8(colorspace);

    /* qoi-data part */
    int run = 0;
    int px_num = width * height;
    uint8_t hash_value;
    int8_t r_delta;
    int8_t g_delta;
    int8_t b_delta;//the difference in three colors(can be negative)
    uint8_t history[64][4];
    memset(history, 0, sizeof(history));
    uint8_t r, g, b, a;
    a = 255u;
    uint8_t pre_r, pre_g, pre_b, pre_a;
    pre_r = 0u;
    pre_g = 0u;
    pre_b = 0u;
    pre_a = 255u;

    for (int i = 0; i < px_num; ++i) {
        r = QoiReadU8();
        g = QoiReadU8();
        b = QoiReadU8();
        if (channels == 4) a = QoiReadU8();
        if (r == pre_r && g == pre_g && b == pre_b) {
            // no difference! consider run mode
            if (run == 61) {
                // we have 62(max) identical values now
                run = 0;
                QoiWriteU8(QOI_OP_RUN_TAG + 61);//run mode written down(62&63 aren't allowed)
            } else {//run mode is still on
                ++run;
            }
        } else {//different value, consider the other four modes
            if (run > 0) {//write the run mode
                QoiWriteU8(QOI_OP_RUN_TAG + run - 1);
                run = 0;//clear the value of run
            }
            hash_value = QoiColorHash(r, g, b);

            if (history[hash_value][0] == r &&
                history[hash_value][1] == g
             && history[hash_value][2] == b) {//index mode
                QoiWriteU8(QOI_OP_INDEX_TAG + hash_value);
            } else {//update hash value
                history[hash_value][0] = r;
                history[hash_value][1] = g;
                history[hash_value][2] = b;

                r_delta = r - pre_r;
                g_delta = g - pre_g;
                b_delta = b - pre_b;
                if (r_delta < 2 && r_delta > -3
                 && g_delta < 2 && g_delta > -3
                 && b_delta < 2 && b_delta > -3) {//difference mode
                    QoiWriteU8(QOI_OP_DIFF_TAG
                               + ((r_delta + 2) << 4)
                               + ((g_delta + 2) << 2)
                               + (b_delta + 2));
                } else if (r_delta - g_delta < 8 && r_delta - g_delta > -9 &&
                           b_delta - g_delta < 8 && b_delta - g_delta > -9 &&
                           g_delta < 32 && g_delta > -33) {//luma mode
                    QoiWriteU8(QOI_OP_LUMA_TAG + g_delta + 32);
                    QoiWriteU8(((r_delta - g_delta + 8) << 4) + ((b_delta - g_delta) + 8));
                } else {//default RGB mode
                    QoiWriteU8(QOI_OP_RGB_TAG);
                    QoiWriteU8(r);
                    QoiWriteU8(g);
                    QoiWriteU8(b);
                }
            }
        }

        pre_r = r;
        pre_g = g;
        pre_b = b;
        pre_a = a;//update the previous value
    }
    if (run > 0) {//we may have the end of run mode not written
        QoiWriteU8(QOI_OP_RUN_TAG + run - 1);
        run = 0;
    }
    // qoi-padding part
    for (int i = 0; i < sizeof(QOI_PADDING) / sizeof(QOI_PADDING[0]); ++i) {
        QoiWriteU8(QOI_PADDING[i]);
    }

    return true;
}

bool QoiDecode(uint32_t &width, uint32_t &height, uint8_t &channels, uint8_t &colorspace) {

    char c1 = QoiReadChar();
    char c2 = QoiReadChar();
    char c3 = QoiReadChar();
    char c4 = QoiReadChar();
    if (c1 != 'q' || c2 != 'o' || c3 != 'i' || c4 != 'f') {
        return false;
    }

    // read image width
    width = QoiReadU32();
    // read image height
    height = QoiReadU32();
    // read channel number
    channels = QoiReadU8();
    // read color space specifier
    colorspace = QoiReadU8();

    int run = 0;
    int px_num = width * height;
    uint8_t hash_value;
    uint8_t history[64][4];
    uint8_t head_name;
    uint8_t extra_luma;
    memset(history, 0, sizeof(history));

    uint8_t r, g, b, a;
    int8_t r_delta, g_delta, b_delta;
    uint8_t luma_tag;
    a = 255u;

    for (int i = 0; i < px_num; ++i) {
        if (run != 0) {//print the same RGB value
            --run;
        } else {//the other four modes(with head tags)
            head_name = QoiReadU8();

            if (head_name == QOI_OP_RGB_TAG) {
                //tackle RGB mode first to avoid coincidence with run mode
                r = QoiReadU8();
                g = QoiReadU8();
                b = QoiReadU8();
            } else {
                if ((head_name & QOI_MASK_2) == QOI_OP_RUN_TAG) {//run mode
                    run = (head_name % 64u);//have the run number
                } else if ((head_name & QOI_MASK_2) == QOI_OP_DIFF_TAG) {//difference mode
                    r += ((head_name & 63) >> 4) - 2;
                    g += ((head_name & 15) >> 2) - 2;
                    b += (head_name & 3) - 2;
                } else if ((head_name & QOI_MASK_2) == QOI_OP_INDEX_TAG) {//index mode
                    hash_value = head_name % 64u;
                    r = history[hash_value][0];
                    g = history[hash_value][1];
                    b = history[hash_value][2];//store index value
                } else if ((head_name & QOI_MASK_2) == QOI_OP_LUMA_TAG) {//luma mode
                    luma_tag = (head_name & 63);
                    g_delta = luma_tag - 32;
                    extra_luma = QoiReadU8();
                    r_delta = (extra_luma >> 4) - 8 + g_delta;
                    b_delta = (extra_luma & 15) - 8 + g_delta;
                    r += r_delta;
                    g += g_delta;
                    b += b_delta;
                }
            }
        }
        hash_value = QoiColorHash(r, g, b);
        history[hash_value][0] = r;
        history[hash_value][1] = g;
        history[hash_value][2] = b;//store index value
        QoiWriteU8(r);
        QoiWriteU8(g);
        QoiWriteU8(b);
        if (channels == 4) QoiWriteU8(a);
    }
    bool valid = true;
    for (int i = 0; i < sizeof(QOI_PADDING) / sizeof(QOI_PADDING[0]); ++i) {
        if (QoiReadU8() != QOI_PADDING[i]) valid = false;
    }

    return valid;
}

#endif // QOI_FORMAT_CODEC_QOI_H_