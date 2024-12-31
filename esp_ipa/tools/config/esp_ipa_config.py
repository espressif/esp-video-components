# SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

import argparse
import sys
import json
from abc import ABCMeta, abstractmethod
from esp_ipa_utils import fatal_error, cfmt_string, dict_object

class ipa_unit_c():
    __metaclass__ = ABCMeta

    def __init__(self, obj, name, type):
        self.name = name
        self.type = type
        self.text = str()
        self.decode(obj)
    
    @abstractmethod
    def decode(self, obj):
        pass
    
    def get_text(self):
        return self.text

class ipa_unit_awb_c(ipa_unit_c):
    @staticmethod
    def decode_awb(name, obj):
        return cfmt_string('''
            static const esp_ipa_awb_config_t s_ipa_awb_%s_config = {
                .min_counted = %d,
                .min_red_gain_step = %0.4f,
                .min_blue_gain_step = %0.4f
            };
            '''
            %(name, obj.min_counted, obj.min_red_gain_step, obj.min_red_gain_step)
        )

    def decode(self, obj):
        self.text = self.decode_awb(self.name, obj)

class ipa_unit_acc_c(ipa_unit_c):
    @staticmethod
    def decode_acc(name, obj):
        def saturation_code(name, obj):
            saturation_text = str()
            for i in obj.saturation:
                saturation_text += ('''
                    {
                        .color_temp = %d,
                        .saturation = %d
                    },'''%(i.color_temp, i.value))
            return saturation_text

        def ccm_code(name, obj):
            ccm_text = str()
            for i in obj.ccm:
                m = i.matrix
                ccm_text += ('''
                    {
                        .color_temp = %d,
                        .ccm = {
                            .matrix = {
                                { %0.4f, %0.4f, %0.4f },
                                { %0.4f, %0.4f, %0.4f },
                                { %0.4f, %0.4f, %0.4f }
                            }
                        }
                    },'''%(i.color_temp, m[0], m[1], m[2],
                           m[3], m[4], m[5], m[6], m[7], m[8])
                )
            return ccm_text

        acc_text = str()
        acc_obj_text = str()
        if hasattr(obj, 'saturation'):
            acc_text += cfmt_string('''
                static const esp_ipa_acc_sat_t s_ipa_acc_sat_%s_config[] = {
                    %s
                };'''%(name, saturation_code(name, obj))
            )
            acc_obj_text += ('''
                .sat_table = s_ipa_acc_sat_%s_config,
                .sat_table_size = ARRAY_SIZE(s_ipa_acc_sat_%s_config),
                '''%(name, name)
            )

        if hasattr(obj, 'ccm'):
            acc_text += cfmt_string('''
                static const esp_ipa_acc_ccm_t s_ipa_acc_ccm_%s_config[] = {
                    %s
                };'''%(name, ccm_code(name, obj))
            )
            acc_obj_text += ('''
                .ccm_table = s_ipa_acc_ccm_%s_config,
                .ccm_table_size = ARRAY_SIZE(s_ipa_acc_ccm_%s_config)
                '''%(name, name)
            )

        acc_text += cfmt_string('''
            static const esp_ipa_acc_config_t s_ipa_acc_%s_config = {
                %s
            };
            '''%(name, acc_obj_text)
        )

        return acc_text

    def decode(self, obj):
        self.text = self.decode_acc(self.name, obj)

class ipa_unit_aen_c(ipa_unit_c):
    @staticmethod
    def decode_aen(name, obj):
        def gamma_code(name, obj):
            gamma_text = str()
            if hasattr(obj.gamma, 'table'):
                gamma_x = str()
                for i in range(0, 16): gamma_x += '%3d, ' %(min((i + 1) * 16, 255))
                
                gamma_y = str()
                for i in obj.gamma.table: gamma_y += '%3d, '%(i)

                gamma_text += ('''
                    .use_gamma_param = false,
                    .gamma = {
                        .x = {
                            %s
                        },
                        .y = {
                            %s
                        }
                    },'''%(gamma_x, gamma_y)
                )
            elif hasattr(obj.gamma, 'param'):
                gamma_text += ('''
                    .use_gamma_param = true,
                    .gamma_param = %0.4f
                    '''%(obj.gamma.param)
                )
            return gamma_text

        def sharpen_code(name, obj):
            sharpen_text = str()
            for i in obj.sharpen:
                p = i.param
                m = p.matrix
                sharpen_text += ('''
                    {
                        .gain = %d,
                        .sharpen = {
                            .h_thresh = %d,
                            .l_thresh = %d,
                            .h_coeff = %0.4f,
                            .m_coeff = %0.4f,
                            .matrix = {
                                {%d, %d, %d},
                                {%d, %d, %d},
                                {%d, %d, %d}
                            }
                        }
                    },'''%(i.gain * 1000, p.h_thresh, p.l_thresh, p.h_coeff, p.m_coeff,
                           m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8])
                )

            return sharpen_text

        def contrast_code(name, obj):
            contrast_text = str()
            for i in obj.contrast:
                contrast_text += ('''
                    {
                        .gain = %d,
                        .contrast = %d
                    },'''%(i.gain * 1000, i.value)
                )
            return contrast_text

        aen_code = str()
        aen_obj_code = str()
        if hasattr(obj, 'gamma'):
            aen_code += cfmt_string('''
                static const esp_ipa_aen_gamma_config_t s_ipa_aen_gamma_%s_config = {
                    %s
                };'''%(name, gamma_code(name, obj))
            )
            aen_obj_code += ('.gamma = &s_ipa_aen_gamma_%s_config,'%(name))

        if hasattr(obj, 'sharpen'):
            aen_code += cfmt_string('''
                static const esp_ipa_aen_sharpen_t s_ipa_aen_sharpen_%s_config[] = {
                    %s
                };'''%(name, sharpen_code(name, obj))
            )
            aen_obj_code += ('''
                .sharpen_table = s_ipa_aen_sharpen_%s_config,
                .sharpen_table_size = ARRAY_SIZE(s_ipa_aen_sharpen_%s_config),
                '''%(name, name)
            )

        if hasattr(obj, 'contrast'):
            aen_code += cfmt_string('''
                static const esp_ipa_aen_con_t s_ipa_aen_con_%s_config[] = {
                    %s
                };'''%(name, contrast_code(name, obj))
            )
            aen_obj_code += ('''
                .con_table = s_ipa_aen_con_%s_config,
                .con_table_size = ARRAY_SIZE(s_ipa_aen_con_%s_config),
                '''%(name, name)
            )

        aen_code += cfmt_string('''
            static const esp_ipa_aen_config_t s_ipa_aen_%s_config = {
                %s
            };'''%(name, aen_obj_code)
        )

        return aen_code

    def decode(self, obj):
        self.text = self.decode_aen(self.name, obj)

class ipa_unit_adn_c(ipa_unit_c):
    @staticmethod
    def decode_adn(name, obj):
        def bf_code(name, obj):
            adn_text = str()
            for i in obj.bf:
                p = i.param
                m = p.matrix
                adn_text += ('''
                    {
                        .gain = %d,
                        .bf = {
                            .level = %d,
                            .matrix = {
                                {%d, %d, %d},
                                {%d, %d, %d},
                                {%d, %d, %d}
                            }
                        }
                    },'''%(i.gain * 1000, p.level, m[0], m[1], m[2],
                           m[3], m[4], m[5], m[6], m[7], m[8])
                )
            return adn_text

        def dm_code(name, obj):
            dm_text = str()
            for i in obj.demosaic:
                dm_text += ('''
                    {
                        .gain = %d,
                        .dm = {
                            .gradient_ratio = %0.4f
                        }
                    },'''%(i.gain * 1000, i.gradient_ratio)
                )
            return dm_text

        adn_code = str()
        adn_obj_code = str()
        if hasattr(obj, 'bf'):
            adn_code += cfmt_string('''
                static const esp_ipa_adn_bf_t s_ipa_adn_bf_%s_config[] = {
                    %s
                };'''%(name, bf_code(name, obj))
            )
            adn_obj_code += ('''
                .bf_table = s_ipa_adn_bf_%s_config,
                .bf_table_size = ARRAY_SIZE(s_ipa_adn_bf_%s_config),
                '''%(name, name)
            )

        if hasattr(obj, 'demosaic'):
            adn_code += cfmt_string('''
                static const esp_ipa_adn_dm_t s_ipa_adn_dm_%s_config[] = {
                    %s
                };'''%(name, dm_code(name, obj))
            )
            adn_obj_code += ('''
                .dm_table = s_ipa_adn_dm_%s_config,
                .dm_table_size = ARRAY_SIZE(s_ipa_adn_dm_%s_config),
                '''%(name, name)
            )

        adn_code += cfmt_string('''
            static const esp_ipa_adn_config_t s_ipa_adn_%s_config = {
                %s
            };
            '''%(name, adn_obj_code)
        )

        return adn_code

    def decode(self, obj):
        self.text = self.decode_adn(self.name, obj)

class ipa_unit_agc_c(ipa_unit_c):
    @staticmethod
    def decode_agc(name, obj): 
        def light_threshold_config_code(name, obj):
            light_threshold_config_text = str()
            for i in obj.light_threshold_priority:
                light_threshold_config_text += ('''
                    {
                        .luma_threshold = %d,
                        .weight_offset = %d,
                    },'''%(i.luma_threshold, i.weight_offset)
                )
            return light_threshold_config_text

        def exposure_code(name, obj):
            exp = obj.exposure
            gain = obj.gain
            exposure_text = ('''
                .exposure_frame_delay = %d,
                .ac_freq = %d,
                .exposure_adjust_delay = %d,
                .gain_frame_delay = %d,
                .min_gain_step = %0.4f,
                .gain_speed = %0.4f,
                '''%(exp.frame_delay, exp.ac_freq, exp.adjust_delay,
                     gain.frame_delay,gain.min_step, obj.speed)
            )
            return exposure_text
        
        def luma_code(name, obj):
            luma_weight_text = str()
            luma = obj.luma_adjust
            for i in luma.weight: luma_weight_text += '%d, '%(i)
            luma_text = ('''
                .luma_low = %d,
                .luma_high = %d,
                .luma_target = %d,
                .luma_low_threshold = %d,
                .luma_low_regions = %d,
                .luma_high_threshold = %d,
                .luma_high_regions = %d,
                .luma_weight_table = {
                    %s
                },
                '''%(luma.target_low, luma.target_high, luma.target,
                     luma.low_threshold, luma.low_regions,
                     luma.high_threshold, luma.high_regions,
                     luma_weight_text)
            )
            return luma_text
        
        def light_priority_code(name, obj):
            METER_MODE_DICT = {
                'high_light_priority': 'ESP_IPA_AGC_METER_HIGHLIGHT_PRIOR',
                'low_light_priority': 'ESP_IPA_AGC_METER_LOWLIGHT_PRIOR',
                'light_threshold_priority': 'ESP_IPA_AGC_METER_LIGHT_THRESHOLD'
            }

            light_priority_text = ('''
                .meter_mode = %s,
                '''%(METER_MODE_DICT[obj.mode])
            )

            if hasattr(obj, 'high_light_priority'):
                hlp = obj.high_light_priority
                light_priority_text += ('''
                    .high_light_prior_config = {
                        .luma_high_threshold = %d,
                        .luma_low_threshold = %d,
                        .weight_offset = %d,
                        .luma_offset = %d
                    },'''%(hlp.high_threshold, hlp.low_threshold, hlp.weight_offset, hlp.luma_offset)
                )

            if hasattr(obj, 'low_light_priority'):
                llp = obj.low_light_priority
                light_priority_text += ('''
                    .low_light_prior_config = {
                        .luma_high_threshold = %d,
                        .luma_low_threshold = %d,
                        .weight_offset = %d,
                        .luma_offset = %d
                    },'''%(llp.high_threshold, llp.low_threshold, llp.weight_offset, llp.luma_offset)
                )
            
            if hasattr(obj, 'light_threshold_priority'):
                light_threshold_priority_obj_text = str()
                if hasattr(obj, 'light_threshold_priority'):
                    light_threshold_priority_obj_text = ('''
                        .light_threshold_config = {
                            .table = s_ipa_agc_meter_light_thresholds_%s,
                            .table_size = ARRAY_SIZE(s_ipa_agc_meter_light_thresholds_%s)
                        },'''%(name, name)
                    )

                light_priority_text += light_threshold_priority_obj_text
        
            return light_priority_text

        agc_code = str()
        agc_obj_code = str()

        if hasattr(obj, 'light_threshold_priority'):
            agc_code += cfmt_string('''
                static const esp_ipa_agc_meter_light_threshold_t s_ipa_agc_meter_light_thresholds_%s[] = {
                    %s
                };'''%(name, light_threshold_config_code(name, obj))
            )


        agc_code += cfmt_string('''
            static const esp_ipa_agc_config_t s_ipa_agc_%s_config = {
                %s
                %s
                %s
            };'''%(name, exposure_code(name, obj),
                   luma_code(name, obj), light_priority_code(name, obj))
        )

        return agc_code

    def decode(self, obj):
        self.text = self.decode_agc(self.name, obj)

class ipa_c(object):
    def __init__(self, name, version):
        self.name = name
        self.decodes = list()
        self.version = version

    def add(self, obj, name, type):
        ipa_unit_lut = {
            'awb': ipa_unit_awb_c,
            'acc': ipa_unit_acc_c,
            'aen': ipa_unit_aen_c,
            'adn': ipa_unit_adn_c,
            'agc': ipa_unit_agc_c
        }

        if type in ipa_unit_lut:
            self.decodes.append(ipa_unit_lut[type](obj, name, type))

    def get_text(self):        
        text_obj = str()
        text = str()
        names = str()

        for i in self.decodes:
            text += i.get_text()
            text_obj += ('.%s = &s_ipa_%s_%s_config,\n'%(i.type, i.type, i.name))
            names += ('\"esp_ipa_%s\",\n'%(i.type))

        text += cfmt_string('''
            static const char *s_ipa_%s_names[] = {
                %s
            };'''%(self.name, names)
        )

        text += cfmt_string('''
            static const esp_ipa_config_t s_ipa_%s_config = {
                .names = s_ipa_%s_names,
                .nums = ARRAY_SIZE(s_ipa_%s_names),
                .version = %d,
                %s
            };'''
            %(self.name, self.name, self.name, self.version, text_obj)
        )

        return text

class ipas_c(object):
    def __init__(self):
        self.ipa_list = list()
        self.license = cfmt_string('''
            /*
             * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
             *
             * SPDX-License-Identifier: ESPRESSIF MIT
             */'''
        )
        self.header = cfmt_string('''
            #include <string.h>
            #include "esp_ipa.h"
            '''
        )

        self.typedef = cfmt_string('''
            typedef struct esp_video_ipa_index {
                const char *name;
                const esp_ipa_config_t *ipa_config;
            } esp_video_ipa_index_t;
            '''
        )

        self.get_config_func = cfmt_string('''
            const esp_ipa_config_t *esp_ipa_pipeline_get_config(const char *name)
            {
                for (int i = 0; i < ARRAY_SIZE(s_video_ipa_configs); i++) {
                    if (!strcmp(name, s_video_ipa_configs[i].name)) {
                        return s_video_ipa_configs[i].ipa_config;
                    }
                }

                return NULL;
            }'''
        )

        self.get_null_config_func = cfmt_string('''
            const esp_ipa_config_t *esp_video_isp_pipeline_get_ipa_config(const char *name)
            {
                return NULL;
            }'''
        )

    def get_config(self):
        if len(self.ipa_list) > 0:
            def ipa_all_text(ipas):
                ipas_text = str()
                for i in ipas:
                    ipas_text += i.get_text()
                return ipas_text

            def ipa_table_code(ipas):
                ipa_table_text = str()
                for i in ipas:
                    ipa_table_text += ('''
                        {
                            .name = \"%s\",
                            .ipa_config = &s_ipa_%s_config
                        },'''%(i.name, i.name)
                    )
                return ipa_table_text

            text = ipa_all_text(self.ipa_list)
            text += cfmt_string('''
                static const esp_video_ipa_index_t s_video_ipa_configs[] = {
                    %s
                };'''%(ipa_table_code(self.ipa_list))
            )
        return text

    def get_func(self):
        text = str()
        if len(self.ipa_list) > 0:
            text = self.get_config_func
        else:
            text = self.get_null_config_func
        return text

    def add(self, ipa):
        self.ipa_list.append(ipa)

    def get_text(self):
        text  = self.license
        text += self.header
        text += self.typedef
        text += self.get_config()
        text += self.get_func()
        
        return text

def ipa_config(version, input, output): 
    if input:
        files = input.split()
        ipas = ipas_c()

        for f in files:
            j = json.loads(open(f, 'r').read())

            
            for k in j:
                if k == 'version':
                    v = j['version']
                    if v != version:
                        raise fatal_error('IPA json configuration file version should be %d'%(version))
                else:
                    ipa = ipa_c(k, version)
                    for i in j[k]: ipa.add(dict_object(j[k][i]), k, i)
                    ipas.add(ipa)

        input_info = cfmt_string('''
            /* Json file: %s */
            '''%(input)
        )
        open(output, 'w').write(ipas.get_text() + input_info)
    else:
        text = cfmt_string('''
            const void *esp_ipa_pipeline_get_config(const char *name)
            {
                return (void *)0;
            }
            '''
        )

        open(output, 'w').write(text)

def main():
    parser = argparse.ArgumentParser(description='IPA configuration generation', prog='ipa_config')
 
    parser.add_argument(
        '--input', '-i',
        help='application binary file name',
        type=str,
        default=None)

    parser.add_argument(
        '--output', '-o',
        help='Output file name with full path',
        type=str,
        default=None)
    
    parser.add_argument(
        '--version', '-v',
        help='Configuration parameters version',
        type=int,
        default=None)    

    args = parser.parse_args()

    ipa_config(args.version, args.input, args.output)

def _main():
    try:
        main()
    except fatal_error as e:
        print('\nA fatal error occurred: %s' % e)
        sys.exit(2)

if __name__ == '__main__':
    _main()

