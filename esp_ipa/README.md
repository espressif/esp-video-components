# Espressif Image Process Algorithm for ISP

Espressif image process algorithm component provides a suit of image process algorithms.[![Component Registry](https://components.espressif.com/components/espressif/esp_ipa/badge.svg)](https://components.espressif.com/components/espressif/esp_ipa)

## 1. Supported Algorithms

| Algorithm | Description | 
|:-:|:-|
| Auto Color Correction | Calculate lens shadow correction parameters, color correction matrix, and saturation value |
| Auto Denoising | Calculate Bayer denoising parameters and demosaic parameters |
| Auto Enhancement | Calculate GAMMA table, sharpen parameters, and contrast value |
| Auto Gain Control | Calculate exposure and gain |
| Auto White Balance | Calculate red and blue channels' gain |
| Image Analyze | Calculate image color temperature and luma |
| Auto Sensor AE Target Control | This is added for the sensors supported AEG/AGC by themselves |

## 2. Pipeline Global Variable

This module is to share variables between algorithm modules without local cache in one image process pipeline, for example:

```
algorithm_1.c

    int color = 15;
    esp_ipa_set_int32(ipa_1, "color", 15);


algorithm_2.c

    if (esp_ipa_has_var(ipa_2, "color")) {
        int color = esp_ipa_get_int32(ipa, "color")
    }

algorithm_3.c

    if (esp_ipa_has_var(ipa_3, "color")) {
        int color = esp_ipa_get_int32(ipa, "color")
    }

......
```

- Note: "ipa_1", "ipa_2" and "ipa_3" should be in one IPA pipeline

## 3. JSON Configuration

Developers can refer to the configuration files in [esp_cam_sensor](https://github.com/espressif/esp-video-components/tree/master/esp_cam_sensor) about the JSON parameters usage:

- [SC2336](https://github.com/espressif/esp-video-components/blob/master/esp_cam_sensor/sensors/sc2336/cfg/sc2336_default.json)
- [OV5647](https://github.com/espressif/esp-video-components/blob/master/esp_cam_sensor/sensors/ov5647/cfg/ov5647_default.json)
- [OV2710](https://github.com/espressif/esp-video-components/blob/master/esp_cam_sensor/sensors/ov2710/cfg/ov2710_default.json)

### 3.1 Global Parameters

---

```json
{
    "version": 1,
    "SC2336": {}
}
```

| Parameter | Type | Range | Description |
|:-:|:-:|:-:|:-|
| version | Integer | >1 | JSON configuration version, this variable adds only when JSON configuration parameters change |
| SC2336 | Object | / | Target sensor name, such as "SC2336", "OV5647" and so on |

---

### 3.2 Algorithm Parameters

#### 3.2.1 Auto White Balance

---

```json
"SC2336":
{
    "awb":
    {
        "model": 0,
        "min_counted": 2000,
        "min_red_gain_step": 0.034,
        "min_blue_gain_step": 0.034,
        "range":
        {
            "green":
            {
                "max": 200,
                "min": 180
            },
            "rg":
            {
                "max": 1.2,
                "min": 0.8
            },
            "bg":
            {
                "max": 1.2,
                "min": 0.8
            }
        },
        "green_luma_env": "awb_green_luma",
        "green_luma_init": 200,
        "green_luma_step_ratio": 0.3
    }
}
```

| Parameter | Type | Range | Description |
|:-:|:-:|:-:|:-|
| awb | Object | / | Auto white balance configuration parameters |
| model | Integer | {0,1} | AWB data process model |
|  min_counted | Integer | >0 | Minimum white points: Only when the white points number is larger than or equal to this does the auto white balance algorithm run |
| min_red_gain_step | Float | >0 | Minimum red channel gain step: Only when the red channel gain step is larger than or equal to this is the gain set into the ISP |
| min_blue_gain_step | Float | >0 | Minimum blue channel gain step: Only when the blue channel gain step is larger than or equal to this is the gain set into the ISP |
| range | Object | / | AWB statistics parameters |
| green | Object | / | AWB green channel statistics parameters |
| max(green) | Integer | <div style="white-space: nowrap;">(0,255)</div> | AWB green channel maximum lumina |
| min(green) | Integer | <div style="white-space: nowrap;">(0,255)</div> | AWB green channel minimum lumina |
| rg | Object | / | AWB red channel and green channel ratio statistics parameters |
| max(rg) | Integer | <div style="white-space: nowrap;">(0,255)</div> | AWB red channel and green channel maximum ratio |
| min(rg) | Integer | <div style="white-space: nowrap;">(0,255)</div> | AWB red channel and green channel minimum ratio |
| bg | Object | / | AWB blue channel and green channel ratio ratio statistics parameters |
| max(bg) | Integer | <div style="white-space: nowrap;">(0,255)</div> | AWB blue channel and green channel maximum ratio |
| min(bg) | Integer | <div style="white-space: nowrap;">(0,255)</div> | AWB blue channel and green channel minimum ratio |
| green_luma_env | String | / | AWB green channel lumina variable name |
| green_luma_init | Integer | <div style="white-space: nowrap;">(0,255)</div> | AWB green channel initialization lumina |
| green_luma_step_ratio | Float | >0 | AWB green channel minimum lumina step ratio |

---

#### 3.2.2 Auto Color Correction

---

```json
"SC2336":
{
    "acc": {}
}
```

| Parameter | Type | Range | Description |
|:-:|:-:|:-:|:-|
| acc | Object | / | Auto color correction configuration parameters |

---

```json
"acc":
{
    "saturation":
    [
        {
            "color_temp": 0,
            "value": 128
        }
        ...
    ],
}
```

| Parameter | Type | Range | Description |
|:-:|:-:|:-:|:-|
| saturation | Array | / | The saturation value and color temperature mapping table adopted the principle of nearest neighbor indexing |
| color_temp | Integer | >0 | Color temperature value |
| value | Integer | <div style="white-space: nowrap;">[0,255]</div> | Saturation value |

---

```json
"acc":
{
    "ccm": {}
}
```

| Parameter | Type | Range | Description |
|:-:|:-:|:-:|:-|
| ccm | Object | / | Color correction matrix configuration parameters |

---

```json
"ccm":
{
    "model": 1,
    "min_ct_step": 500
}
```

| Parameter | Type | Range | Description |
|:-:|:-:|:-:|:-|
| model | Integer | {0,1} | CCM data process model |
| min_ct_step | Integer | >0 | Color temperature minimum step value for generating new CCM value  |

---

```json
"ccm":
{
    "low_luma":
    {
        "luma_env": "ae.luma.avg",
        "threshold": 28,
        "matrix":
        [
            1.00,  0.00,  0.00,
            0.00,  1.00,  0.00,
            0.00,  0.00,  1.00
        ]
    },
}
```

| Parameter | Type | Range | Description |
|:-:|:-:|:-:|:-|
| low_luma | Object | / | Color correction matrix configuration parameters in low brightness scenes |
| luma_env | String | / | Lumina variable name |
| threshold | Float | >0 | Minimum brightness: if the "luma_env" value is less than this is the low brightness CCM value set into the ISP |
| matrix | Array | <div style="white-space: nowrap;">ESP32-P4: (-4,4)</div> | Low brightness CCM value |

---

```json
"ccm":
{
    "table":
    [
        {
            "color_temp": 2320,
            "matrix":
            [
                 2.0000, -0.1680, -0.8320,
                -0.3716,  2.0000, -0.6284,
                -0.7150, -0.2850,  2.0000
            ]
        },
        ...
    ]
}
```

| Parameter | Type | Range | Description |
|:-:|:-:|:-:|:-|
| table | Array | / | The CCM and color temperature mapping table adopted the principle of nearest neighbor indexing |
| color_temp | Integer | >0 | Color temperature value |
| matrix | Array | <div style="white-space: nowrap;">ESP32-P4: (-4,4)</div> | CCM value |

---

```json
"acc":
{
    "lsc": {}
}
```

| Parameter | Type | Range | Description |
|:-:|:-:|:-:|:-|
| lsc | Object | / | Lens shadow correction(LSC) configuration parameters |

---

```json
"lsc":
{
    "model": 0,
    "img_w": 1920,
    "img_h": 1080,
    "lsc_tbl_size": 558,
    "table":
    [
        {
            "ct": 3350,
            "calibrations_r_tbl": [ 2.13310074, ... ],
            "calibrations_gr_tbl": [ 1.92501747, ... ],
            "calibrations_gb_tbl": [ 1.90096950, ... ],
            "calibrations_b_tbl": [ 1.67257392, ... ],
        },
        ...
    ]
}
```

| Parameter | Type | Range | Description |
|:-:|:-:|:-:|:-|
| model | Integer | 0 | LSC data process model |
| img_w | Integer | >0 | Image resolution width |
| img_h | Integer | >0 | Image resolution height |
| lsc_tbl_size | Integer | >0 | LSC data array size, different image resolution has different values, such as "lsc_tbl_size" of 1920x1080 resolution is 558 |
| table | Array | / | The LSC array data and color temperature mapping table adopted the principle of nearest neighbor indexing |
| ct | Integer | >0 | Color temperature value |
| calibrations_r_tbl | Array[Float] | <div style="white-space: nowrap;">ESP32-P4: (-4,4)</div> | LSC data array for the red channel in RAW field |
| calibrations_gr_tbl | Array[Float] | <div style="white-space: nowrap;">ESP32-P4: (-4,4)</div> | LSC data array for the red-green(RG) channel in RAW field |
| calibrations_gb_tbl | Array[Float] | <div style="white-space: nowrap;">ESP32-P4: (-4,4)</div> | LSC data array for the blue-green(BG) channel in RAW field |
| calibrations_b_tbl | Array[Float] | <div style="white-space: nowrap;">ESP32-P4: (-4,4)</div> | LSC data array for the blue channel in RAW field |

---

#### 3.2.3 Auto Denoising

---

```json
"SC2336":
{
    "adn": {}
}
```

| Parameter | Type | Range | Description | 
|:-:|:-:|:-:|:-|
| adn | Array | / | Auto denoising configuration parameters |

---

```json
"adn":
{
    "bf":
    [
        {
            "gain": 1,
            "param":
            {
                "level": 3,
                "matrix":
                [
                    1, 3, 1,
                    3, 5, 3,
                    1, 3, 1
                ],
                "sigma": 1
            }
        },
        ...
    ]
}
```

| Parameter | Type | Range | Description |
|:-:|:-:|:-:|:-|
| bf | Array | / | The Bayer filter parameters and sensor gain mapping table adopted the principle of nearest neighbor indexing |
| gain | Float | >0 | Sensor gain |
| param | Object | / | Bayer filter parameters |
| level | Integer | <div style="white-space: nowrap;">ESP32-P4: [2,20]</div> | Bayer filter sigma |
| matrix | Integer | ESP32-P4: [0,15] | Bayer filter matrix |
| sigma | Float | >0 | Sigma parameter to generate Gaussian matrix |

* Note: The Bayer filter is a bilateral filter, level is a parameter of range kernel, and matrix is a parameter of spatial kernel
* Note: If there is a matrix, the JSON tools use it firstly, otherwise, use sigma

---

```json
"adn":
{
    "demosaic":
    [
        {
            "gain": 1,
            "gradient_ratio": 1.0
        },
        ...
    ]
}
```

| Parameter | Type | Range | Description |
|:-:|:-:|:-:|:-|
| demosaic | Array | / | The Demosaic parameters and sensor gain mapping table adopted the principle of nearest neighbor indexing |
| gain | Float | >0 | Sensor gain |
| gradient_ratio | Float | <div style="white-space: nowrap;">ESP32-P4: (0,4)</div> | Demosaic grad ratio parameters |

---

#### 3.2.4 Auto Enhancement

---

```json
"SC2336":
{
    "aen": {}
}
```

| Parameter | Type | Range | Description | 
|:-:|:-:|:-:|:-|
| aen | Object | / | Auto enhancement configuration parameters |

---

```json
"aen":
{
    "gamma":
    {
        "model": 0,
        "use_gamma_param": true,
        "luma_env": "ae.luma.avg",
        "luma_min_step": 16.0,
    },
}
```

| Parameter | Type | Range | Description | 
|:-:|:-:|:-:|:-|
| model | Integer | {0,1} | GAMMA data process model |
| gamma | Object | / | GAMMA configuration parameters object |
| use_gamma_param | bool | true/false | true: use the given gamma parameter to generate gamma Y table; false: use the given gamma Y table |
| luma_env | String | / | Lumina variable name |
| luma_min_step | Float | >0 | Minimum brightness step: if the "luma_env" step value is larger than this is the new GAMMA table value set into the ISP |

---

```json
"gamma":
{
    "table":
    [
        {
            {
                "luma": 31.1,
                "gamma_param": 0.51,
                "y":
                [
                    0, 64, 91, 112, 130, 146, 160, 173, 185, 197, 207, 218, 228, 237, 246, 255
                ]
            },
        },
        ...
    ]
}
```

| Parameter | Type | Range | Description | 
|:-:|:-:|:-:|:-|
| table | Array | / | The GAMMA and luma mapping table adopted the principle of linear interpolation |
| luma | Float | >0 | Luma index value |
| gamma_param | Float | >0 | Parameter to generate GAMMA Y array |
| y | <div style="white-space: nowrap;">Array[Integer]</div> | <div style="white-space: nowrap;">ESP32-P4: [0,255]</div> | GAMMA Y array |

---

```json
"aen"
{
    "sharpen":
    [
        {
            "gain": 1,
            "param":
            {
                "h_thresh": 16,
                "l_thresh": 3,
                "h_coeff": 2.65,
                "m_coeff": 2.95,
                "matrix":
                [
                    1, 2, 1,
                    2, 2, 2,
                    1, 2, 1
                ]
            }
        },
        ...
    ]
}
```

| Parameter | Type | Range | Description | 
|:-:|:-:|:-:|:-|
| sharpen | Array | / | The sharpen parameters and sensor gain mapping table adopted the principle of nearest neighbor indexing |
| gain | Float | >0 | Sensor gain |
| param | Object | / | Sharpen parameters |
| h_thresh | Integer | <div style="white-space: nowrap;">ESP32-P4: [0,255]</div> | Sharpen high threshold, filtered pixel value higher than this threshold multiplies by `h_coeff` |
| l_thresh | Integer | ESP32-P4: [0,255] | Sharpen low threshold, filtered pixel value higher than this threshold but lower than `h_thresh` will be multiplied by `m_coeff` and filtered pixel value lower than this threshold will be set to 0 |
| h_coeff | Float | ESP32-P4: (0,8) | High frequency pixel sharpness coefficient |
| m_coeff | Float | ESP32-P4: (0,8) | Medium frequency pixel sharpness coefficient |
| matrix | <div style="white-space: nowrap;">Array [Integer]</div> | ESP32-P4: [0,31] | 3x3 low pass filter matrix data |

---

```json
"aen":
{
    "contrast":
    [
        {
            "gain": 1,
            "value": 130
        },
        ...
    ]
}
```

| Parameter | Type | Range | Description | 
|:-:|:-:|:-:|:-|
| contrast | Array | / | The contrast value and sensor gain mapping table adopted the principle of nearest neighbor indexing |
| gain | Float | >0 | Sensor gain |
| value | Integer | <div style="white-space: nowrap;">ESP32-P4: [0,255]</div> | Contrast value |

---

#### 3.2.5 Auto Gain Control

---

```json
"SC2336":
{
    "agc": {}
}
```

| Parameter | Type | Range | Description | 
|:-:|:-:|:-:|:-|
| agc | Object | / | Auto gain/exposure control configuration parameters |

---

```json
"agc":
{
    "exposure":
    {
        "frame_delay": 2,
        "adjust_delay": 0
    }
}
```

| Parameter | Type | Range | Description | 
|:-:|:-:|:-:|:-|
| exposure | Object | / | Exposure control configuration parameters object |
| frame_delay | Integer | >0 | The delayed frames number of setting exposure parameter taking effect, this value is from sensor hardware information |
| adjust_delay | Integer | >0 | The delayed frames number of calculating and setting new exposure parameter, higher value makes brightness change slower |

---

```json
"agc":
{
    "gain":
    {
        "min_step": 0.0001,
        "frame_delay": 2
    }
}
```

| Parameter | Type | Range | Description | 
|:-:|:-:|:-:|:-|
| gain | Object | / | Gain control configuration parameters object |
| frame_delay | Integer | >0 | The delayed frames number of setting gain parameter taking effect, this value is from sensor hardware information |
| min_step | Float | >0 | Minimum gain step: Only when the gain step is larger than or equal to this value is the gain set into the sensor |

---

```json
"agc":
{
    "anti_flicker":
    {
        "mode": "full",
        "ac_freq": 50
    },
}
```

| Parameter | Type | Range | Description | 
|:-:|:-:|:-:|:-|
| anti_flicker | Object | / | Anti-flicker control configuration parameters object |
| mode | String enumeration | "full"<hr>"part"<hr>"none" | `"full"`: force to use anti-flicker exposure configuration <hr>`"part"`: if the gain can adjust the image brightness, force to use anti-flicker exposure configuration, otherwise change the exposure ignore anti-flicker <hr> `"none"`: adjust exposure and gain fully ignore anti-flicker |
| ac_freq | Integer | >0 | Alternating current frequency for lighting, in general, the value is 50Hz or 60Hz |

---

```json
"agc":
{
    "f_n0": 0.32,
    "f_m0": 0.32
}
```

| Parameter | Type | Range | Description | 
|:-:|:-:|:-:|:-|
| f_n0 | Float | (0,1] | The parameter to control gain or exposure increasing speed, with a higher value, makes the increasing speed faster |
| f_m0 | Float | (0,1] | The parameter to control gain or exposure decreasing speed, with a higher value, makes the decreasing speed faster |

---

```json
"agc":
{
    "luma_adjust":
    {
        "target_low": 105,
        "target_high": 121,
        "target": 113,
        "low_threshold": 10,
        "low_regions": 5,
        "high_threshold": 240,
        "high_regions": 3,
        "weight":
        [
            1, 1, 2, 1, 1,
            1, 2, 3, 2, 1,
            1, 3, 5, 3, 1,
            1, 2, 3, 2, 1,
            1, 1, 2, 1, 1
        ]
    }
}
```

| Parameter | Type | Range | Description | 
|:-:|:-:|:-:|:-|
| luma_adjust | Object | / | Image brightness control configuration parameter |
| target_low | Integer | (0,target) | Image brightness low threshold: if the image brightness is less than this value does the AGC algorithm calculate and set exposure and gain into the sensor |
| target_high | Integer | (target,255) | Image brightness high threshold: if the image brightness is larger than this value does the AGC algorithm calculate and set exposure and gain into the sensor |
| target | Integer | [3,252] | Image brightness target value: if the image brightness is larger than the "target_high" or lower than the "target_low" does the AGC algorithm calculate exposure and gain to make the image brightness nearest to this value |
| low_threshold | Integer | [3,252] | Image brightness sampled data low threshold: if the image brightness sampled data is less than this value is the low threshold counter increased by 1 |
| high_threshold | Integer | [3,252] | Image brightness sampled data high threshold: if the image brightness sampled data is higher than this value is the high threshold counter increased by 1 |
| low_regions | Integer | <div style="white-space: nowrap;">ESP32-P4:(0,25)</div> | Image low threshold counter low regions number: if low threshold counter is less than this value will all sampled data which is less than "low_threshold" be dropped |
| high_regions | Integer | ESP32-P4:(0,25) | Image high threshold counter high regions number: if high threshold counter is less than this value will all sampled data which is higher than "high_threshold" be dropped |
| weight | <div style="white-space: nowrap;">Array[Integer]</div> | [0,255] | Image brightness sampling weight |

---

```json
"agc":
{
    "mode": "high_light_priority",
}
```

| Parameter | Type | Range | Description | 
|:-:|:-:|:-:|:-|
| mode | String enumeration |"high_light_priority"<hr>"low_light_priority"<hr>"light_threshold_priority" | Measurement weight calculation mode |

---

```json
"agc":
{
    "high_light_priority":
    {
        "low_threshold": 141,
        "high_threshold": 204,
        "weight_offset": 5,
        "luma_offset": -3
    }
}
```

| Parameter | Type | Range | Description | 
|:-:|:-:|:-:|:-|
| high_light_priority | Object | / | High light brightness priority configuration |
| low_threshold | Integer | <div style="white-space: nowrap;">[3,252]</div> | Image brightness sampled data low threshold: if one image brightness sampled data is higher than this value will the data's weight and total weight be increased by weight_offset |
| high_threshold | Integer | [3,252] | Image brightness sampled data high threshold: if one image brightness sampled data is less than this value will the data's weight and total weight be increased by weight_offset |
| weight_offset | Integer | [0,255] | Image brightness sampled data weight increasing value |
| luma_offset | Integer | [-127,127] | Image brightness target increasing value if one sampled data value is between "low_threshold" and "high_threshold" |

* Note: if "mode" is "high_light_priority" this configuration applies

---

```json
"agc":
{
    "low_light_priority":
    {
        "low_threshold": 65,
        "high_threshold": 104,
        "weight_offset": 5,
        "luma_offset": 1
    }
}
```

| Parameter | Type | Range | Description | 
|:-:|:-:|:-:|:-|
| high_light_priority | Object | / | Low light brightness priority configuration |
| low_threshold | Integer | <div style="white-space: nowrap;">[3,252]</div> | Image brightness sampled data low threshold: if one image brightness sampled data is higher than this value will the data's weight and total weight be increased by weight_offset |
| high_threshold | Integer | [3,252] | Image brightness sampled data high threshold: if one image brightness sampled data is less than this value will the data's weight and total weight be increased by weight_offset |
| weight_offset | Integer | [0,255] | Image brightness sampled data weight increasing value |
| luma_offset | Integer | [-127,127] | Image brightness target increasing value if one sampled data value is between "low_threshold" and "high_threshold" |

* Note: if "mode" is "low_light_priority" this configuration applies

---

```json
"agc":
{
    "light_threshold_priority":
    [
        {
            "luma_threshold": 20,
            "weight_offset": 1
        },
        ...
    ]
}
```

| Parameter | Type | Range | Description | 
|:-:|:-:|:-:|:-|
| high_light_priority | Object | / |  Light brightness threshold priority configuration |
| luma_threshold | Integer | <div style="white-space: nowrap;">[0,255]</div> | Light brightness threshold, if one sampled data is larger than this value and less than the next the data's weight and the total weight will be increased by weight_offset |
| weight_offset | Integer | [0,255] | Image brightness sampled data weight increasing value |

* Note: if "mode" is "light_threshold_priority" this configuration applies

---

#### 3.2.6 Auto Sensor AE Target Control

---

```json
"OV2710":
{
    "atc": {}
}
```

| Parameter | Type | Range | Description |
|:-:|:-:|:-:|:-|
| atc | Object | / | Auto sensor AE target control configuration parameters |

---

```json
"atc":
{
    "model": 0,
    "init_value": 48,
    "delay_frames": 5,
    "luma_env": "env.luma.avg",
    "min_ae_value_step": 5,
    "luma_lut":
    [
        {
            "luma": 100,
            "ae_value": 75
        },
        {
            "luma": 200,
            "ae_value": 100
        },
        {
            "luma": 300,
            "ae_value": 150
        }
    ]
}
```
| Parameter | Type | Range | Description |
|:-:|:-:|:-:|:-|
| model | Integer | {0} | ATC data process model |
| init_value | Integer | >0 | Auto sensor AE target control initialization value |
| delay_frames | Integer | >0 | Frames number btween AE target level changing |
| luma_env | String | / | Lumina variable name |
| min_step | Integer | >0 | Minimum AE target level step: Only when the AE target level step is larger than or equal to this value is the AE target level set into the sensor |
| luma_lut | Array | / | The luma value and AE target level mapping table adopted the principle of linear interpolation |
| luma | Integer | >0 | Luma value threshold |
| ae_value | Integer | >0 | AE target level value |
---

#### 3.2.7 Auto Focus Control

---

```json
"OV5647":
{
    "af": {}
}
```

| Parameter | Type | Range | Description |
|:-:|:-:|:-:|:-|
| af | Object | / | Auto focus control configuration parameters |

---

```json
"af":
{
    "model": 0,
    "windows":
 [
 {
            "left": 680,
            "top": 300,
            "width": 390,
            "height": 410
 }
 ],
    "edge_thresh": 32,
    "definition_high_threshold_ratio": 1.3,
    "definition_low_threshold_ratio": 0.7,
    "luminance_high_threshold_ratio": 1.1,
    "luminance_low_threshold_ratio": 0.9,
    "max_change_time": 2000000,
    "l1_scan_points_num": 10,
    "l2_scan_points_num": 10,
    "max_pos": 500
}
```

| Parameter | Type | Range | Description |
|:-:|:-:|:-:|:-|
| model | Integer | 0 | AF data process model |
| windows | Array | / | AF statistics window region, ESP32-P4 supports 3 windows |
| left | Integer | / | Window Left coordinate  |
| top | Integer | / | Window Top coordinate |
| width | Integer | / | Window width |
| height | Integer | / | Window height |
| edge_thresh | Integer | / | AF statistics threshold, and value less will be dropped |
| definition_high_threshold_ratio | Float | >1.0 | When the (current definition statistics)/(previous focus definition) ratio is larger than this value, a new AF process will trigger |
| definition_low_threshold_ratio | Float | <1.0 | When the (current definition statistics)/(previous focus definition) ratio is less than this value, a new AF process will trigger |
| luminance_high_threshold_ratio | Float | >1.0 | When the (current luminance statistics)/(previous focus luminance) ratio is larger than this value, a new AF process will trigger |
| luminance_low_threshold_ratio | Float | <1.0 | When the (current luminance statistics)/(previous focus luminance) ratio is less than this value, a new AF process will trigger |
| max_change_time | Integer | >0 | The time how long the AF statistics definition or luminance keeps out of the threshold, the unit is microsecond |
| l1_scan_points_num | Integer | >0 | Level 1 scanning point numbers |
| l2_scan_points_num | Integer | >0 | Level 2 scanning point numbers |
| max_pos | Integer | >0 | Maximum focus position, less value can reduce scanning time cost, but also shorten the focus distance |

---

#### 3.2.8 Extended Control

---

```json
"OV5647":
{
    "ext": {}
}
```

| Parameter | Type | Range | Description |
|:-:|:-:|:-:|:-|
| ext | Object | / | Extended control configuration parameters |

---

```json
"ext":
{
    "hue": 0,
    "brightness": 0,
    "stats_region":
    {
        "left": 100,
        "top": 100,
        "width": 400,
        "height": 400
    }
}
```

| Parameter | Type | Range | Description |
|:-:|:-:|:-:|:-|
| hue | Integer | [0,360] | ISP color HUE  |
| brightness | Integer | [-128,127] | ISP color brightness |
| stats_region | Object | / | ISP statistics region |
| left | Integer | / | Window Left coordinate  |
| top | Integer | / | Window Top coordinate |
| width | Integer | / | Window width |
| height | Integer | / | Window height |
