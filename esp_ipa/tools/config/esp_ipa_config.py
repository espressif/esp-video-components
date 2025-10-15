# SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

import argparse
import sys
import json
import math
from abc import ABCMeta, abstractmethod
from esp_ipa_utils import fatal_error, cfmt_string, dict_object

class base_default(object):
    def __init__(self):
        pass

class awb_range_default():
    def __init__(self, name):
        self.green = base_default()
        self.rg = base_default()
        self.bg = base_default()

        self.green.max = 220
        self.green.min = 180
        self.rg.max = 0.8899
        self.rg.min = 0.5040
        self.bg.max = 0.7822
        self.bg.min = 0.4838

class awb_default():
    def __init__(self, name):
        self.model = 0
        self.range = awb_range_default(name)
        self.min_counted = 1000
        self.green_luma_env = 'ae.luma.avg'
        self.green_luma_init = 200
        self.green_luma_step_ratio = 0.3

class ext_default():
    def __init__(self, name):
        self.hue = 0
        self.brightness = 0
        self.stats_region = base_default()
        self.stats_region.left = 0
        self.stats_region.top = 0
        self.stats_region.width = 0
        self.stats_region.height = 0

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

    def customized_name(self):
        return None

    def get_text(self):
        return self.text

class ipa_unit_ian_c(ipa_unit_c):
    @staticmethod
    def decode_ian(name, obj):
        def ct_code(name, obj):
            ct_bp_text = str()
            for i in obj.bp:
                ct_bp_text += ('''
                    {
                        .a0 = %f,
                        .a1 = %f
                    },'''%(i.a0, i.a1)
                )

            ct_text = cfmt_string('''
                static const esp_ipa_ian_ct_basic_param_t s_ipa_ian_ct_%s_basic_param[] = {
                    %s
                };'''%(name, ct_bp_text)
            )

            g_a2_obj_text = str()
            for i in obj.g.a2:
                g_a2_obj_text += '%f, '%(i)

            ct_text += cfmt_string('''
                static const float s_esp_ipa_ian_ct_%s_g_a2[] = {
                    %s
                };'''%(name, g_a2_obj_text)
            )

            m_a0 = 0
            m_a1 = 0
            m_a2 = 0
            min_step = 0
            if obj.model == 1:
                m_a0 = obj.m_1.a0
                m_a1 = obj.m_1.a1
                min_step = obj.min_step
            elif obj.model == 2:
                m_a0 = obj.m_2.a0
                m_a1 = obj.m_2.a1
                m_a2 = obj.m_2.a2
                min_step = obj.min_step

            ct_text += cfmt_string('''
                static const esp_ipa_ian_ct_config_t s_esp_ipa_ian_ct_%s_config = {
                    .model = %d,
                    .m_a0 = %f,
                    .m_a1 = %f,
                    .m_a2 = %f,
                    .f_n0 = %f,
                    .bp = s_ipa_ian_ct_%s_basic_param,
                    .bp_nums = ARRAY_SIZE(s_ipa_ian_ct_%s_basic_param),
                    .min_step = %d,
                    .g_a0 = %f,
                    .g_a1 = %f,
                    .g_a2 = s_esp_ipa_ian_ct_%s_g_a2,
                    .g_a2_nums = ARRAY_SIZE(s_esp_ipa_ian_ct_%s_g_a2)       
                };
                '''%(name, obj.model, m_a0, m_a1, m_a2,
                     obj.f_n0, name, name, min_step,
                     obj.g.a0, obj.g.a1, name, name)
            )

            return ct_text
        
        def luma_code(name, obj):
            luma_text = ''
            luma_sub_text = ''

            if hasattr(obj, 'histogram'):
                hist = obj.histogram

                mean_text = ''
                for m in hist.mean:
                    mean_text += '%d,'%(m)

                luma_text += cfmt_string('''
                    static const esp_ipa_ian_luma_hist_config_t s_esp_ipa_ian_luma_hist_%s_config = {                 
                        .mean = {
                            %s
                        },
                        .low_index_start = %d,
                        .low_index_end = %d,
                        .high_index_start = %d,
                        .high_index_end = %d,
                        .back_light_radio_threshold = %0.4f,
                    }; 
                    '''%(name, mean_text, hist.low_index.start, hist.low_index.end,
                         hist.high_index.start, hist.high_index.end, hist.back_light_radio_threshold)
                )

                luma_sub_text += cfmt_string('''
                   .hist = &s_esp_ipa_ian_luma_hist_%s_config,
                    '''%(name)
                )
            
            if hasattr(obj, 'ae'):
                ae = obj.ae

                weight_text = ''
                for w in ae.weight:
                    weight_text += '%d,'%(w)
                
                luma_text += cfmt_string('''
                    static const esp_ipa_ian_luma_ae_config_t s_esp_ipa_ian_luma_ae_%s_config = {                 
                        .weight = {
                            %s
                        },
                    };'''%(name, weight_text)
                )

                luma_sub_text += cfmt_string('''
                   .ae = &s_esp_ipa_ian_luma_ae_%s_config,
                    '''%(name)
                )

            if hasattr(obj, 'env'):
                env = obj.env

                if len(env.weight) != 25:
                    fatal_error('env.weight size must be 25, but got %d'%(len(env.weight)))

                weight_text = ''
                for w in env.weight:
                    weight_text += '%d,'%(w)

                def speed_param_code(name, obj):
                    speed_param_text = str()
                    for i in obj.speed_param:
                        speed_param_text += '%f, '%(i)
                    return speed_param_text

                luma_text += cfmt_string('''
                    static const float s_esp_ipa_ian_luma_env_speed_param_%s_config[] = {
                        %s
                    };'''%(name, speed_param_code(name, env))
                )

                luma_text += cfmt_string('''
                    static const esp_ipa_ian_luma_env_config s_esp_ipa_ian_luma_env_%s_config = {
                        .k = %f,
                        .speed_param = s_esp_ipa_ian_luma_env_speed_param_%s_config,
                        .speed_param_size = ARRAY_SIZE(s_esp_ipa_ian_luma_env_speed_param_%s_config),
                        .weight = {
                            %s
                        },
                    };'''%(name, env.k, name, name, weight_text)
                )

                luma_sub_text += cfmt_string('''
                   .env = &s_esp_ipa_ian_luma_env_%s_config,
                    '''%(name)
                )

            luma_text += cfmt_string('''
                static const esp_ipa_ian_luma_config_t s_esp_ipa_ian_luma_%s_config = {
                    %s
                };'''%(name, luma_sub_text)
            )

            return luma_text

        ian_text = str()
        ian_obj_text = str()
        if hasattr(obj,'color_temp'):
            ian_text += ct_code(name, obj.color_temp)
            ian_obj_text += cfmt_string('''
                .ct = &s_esp_ipa_ian_ct_%s_config,
                '''%(name)
            )

        if hasattr(obj, 'luma'):
            ian_text += luma_code(name, obj.luma)
            ian_obj_text += cfmt_string('''
                .luma = &s_esp_ipa_ian_luma_%s_config,
                '''%(name)
            )

        ian_text += cfmt_string('''
            static const esp_ipa_ian_config_t s_ipa_ian_%s_config = {
                %s
            };'''%(name, ian_obj_text)
        )

        return ian_text

    def decode(self, obj):
        self.text = self.decode_ian(self.name, obj)

class ipa_unit_awb_c(ipa_unit_c):
    @staticmethod
    def decode_awb(name, obj):
        defconf = awb_default(name)

        if not hasattr(obj, 'model'):
            obj.model = defconf.model
        if not hasattr(obj, 'min_counted'):
            obj.min_counted = defconf.min_counted
        if not hasattr(obj, 'range'):
            obj.range = defconf.range
        if not hasattr(obj, 'green_luma_env'):
            obj.green_luma_env = defconf.green_luma_env
        if not hasattr(obj, 'green_luma_init'):
            obj.green_luma_init = defconf.green_luma_init
        if not hasattr(obj, 'green_luma_step_ratio'):
            obj.green_luma_step_ratio = defconf.green_luma_step_ratio

        model_dict = {
            0: 'ESP_IPA_AWB_MODEL_0',
            1: 'ESP_IPA_AWB_MODEL_1'
        }

        return cfmt_string('''
            static const esp_ipa_awb_config_t s_ipa_awb_%s_config = {
                .model = %s,
                .min_counted = %d,
                .min_red_gain_step = %0.4f,
                .min_blue_gain_step = %0.4f,
                .range = {
                    .green_max = %d,
                    .green_min = %d,
                    .rg_max = %0.4f,
                    .rg_min = %0.4f,
                    .bg_max = %0.4f,
                    .bg_min = %0.4f
                },
                .green_luma_env = \"%s\",
                .green_luma_init = %d,
                .green_luma_step_ratio = %0.4f
            };
            '''
            %(name, model_dict[obj.model], obj.min_counted, obj.min_red_gain_step, obj.min_red_gain_step,
              obj.range.green.max, obj.range.green.min, obj.range.rg.max, obj.range.rg.min,
              obj.range.bg.max, obj.range.bg.min, obj.green_luma_env, obj.green_luma_init, obj.green_luma_step_ratio)
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
            ccm = obj.ccm
            ccm_table_sub_text = str()
            for i in ccm.table:
                m = i.matrix
                ccm_table_sub_text += ('''
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

            m = ccm.low_luma.matrix
            ccm_low_table_text = cfmt_string('''
                    {
                        .matrix = {
                            { %0.4f, %0.4f, %0.4f },
                            { %0.4f, %0.4f, %0.4f },
                            { %0.4f, %0.4f, %0.4f }
                        }
                    }
                '''%(m[0], m[1], m[2],
                     m[3], m[4], m[5],
                     m[6], m[7], m[8])
            )

            ccm_table_text = cfmt_string('''
                static const esp_ipa_acc_ccm_unit_t s_esp_ipa_acc_ccm_%s_table[] = {
                    %s
                };
                '''%(name, ccm_table_sub_text)                        
            )

            if not hasattr(ccm, 'model'):
                ccm.model = 0

            sub_param_text = str()
            if ccm.model == 1:
                sub_param_text = cfmt_string('''
                    .min_ct_step = %d,
                    '''%(ccm.min_ct_step)
                )

            ccm_text = cfmt_string('''
                static const esp_ipa_acc_ccm_config_t s_esp_ipa_acc_ccm_%s_config = {
                    .model = %d,
                    .luma_env = \"%s\",
                    .luma_low_threshold = %0.4f,
                    .luma_low_ccm = %s,
                    .ccm_table = s_esp_ipa_acc_ccm_%s_table,
                    .ccm_table_size = %d,
                    %s
                };'''%(name, ccm.model, ccm.low_luma.luma_env, ccm.low_luma.threshold,
                       ccm_low_table_text, name, len(ccm.table), sub_param_text)
            )

            return ccm_table_text + ccm_text

        def lsc_code(name, obj):
            lsc = obj.lsc
            lsc_table_text = str()
            lsc_text = str()

            for i in lsc.table:
                lsc_text += cfmt_string('''
                    static const isp_lsc_gain_t s_esp_ipa_acc_lsc_gain_r_%s_%d_x_%d_ct_%d_config[] = {
                        %s
                    };
                    '''%(name, lsc.img_w, lsc.img_h, i.ct,
                         ', '.join('{.val = %d}'%(round(u * 256)) for u in i.calibrations_r_tbl))
                )

                lsc_text += cfmt_string('''
                    static const isp_lsc_gain_t s_esp_ipa_acc_lsc_gain_gr_%s_%d_x_%d_ct_%d_config[] = {
                        %s
                    };
                    '''%(name, lsc.img_w, lsc.img_h, i.ct,
                         ', '.join('{.val = %d}'%(round(u * 256)) for u in i.calibrations_gr_tbl))
                )

                lsc_text += cfmt_string('''
                    static const isp_lsc_gain_t s_esp_ipa_acc_lsc_gain_gb_%s_%d_x_%d_ct_%d_config[] = {
                        %s
                    };
                    '''%(name, lsc.img_w, lsc.img_h, i.ct,
                         ', '.join('{.val = %d}'%(round(u * 256)) for u in i.calibrations_gb_tbl))
                )

                lsc_text += cfmt_string('''
                    static const isp_lsc_gain_t s_esp_ipa_acc_lsc_gain_b_%s_%d_x_%d_ct_%d_config[] = {
                        %s
                    };
                    '''%(name, lsc.img_w, lsc.img_h, i.ct,
                         ', '.join('{.val = %d}'%(round(u * 256)) for u in i.calibrations_b_tbl))
                )

                lsc_table_text += cfmt_string('''
                    {
                        .color_temp = %d,
                        .lsc = {
                            .gain_r  = s_esp_ipa_acc_lsc_gain_r_%s_%d_x_%d_ct_%d_config,
                            .gain_gr = s_esp_ipa_acc_lsc_gain_gr_%s_%d_x_%d_ct_%d_config,
                            .gain_gb = s_esp_ipa_acc_lsc_gain_gb_%s_%d_x_%d_ct_%d_config,
                            .gain_b  = s_esp_ipa_acc_lsc_gain_b_%s_%d_x_%d_ct_%d_config,
                            .lsc_gain_array_size = %d
                        },
                    },
                    '''%(i.ct,
                         name, lsc.img_w, lsc.img_h, i.ct, name, lsc.img_w, lsc.img_h, i.ct,
                         name, lsc.img_w, lsc.img_h, i.ct, name, lsc.img_w, lsc.img_h, i.ct,
                         lsc.lsc_tbl_size)
                )

            lsc_text += cfmt_string('''
                static const esp_ipa_acc_lsc_lut_t s_esp_ipa_acc_lsc_%s_%d_x_%d_config[] = {
                    %s
                };
                '''%(name, lsc.img_w, lsc.img_h, lsc_table_text)
            )

            lsc_text += cfmt_string('''
                static const esp_ipa_acc_lsc_t s_esp_ipa_acc_lsc_%s_config[] = {
                    {
                        .width = %d,
                        .height = %d,
                        .lsc_gain_table = s_esp_ipa_acc_lsc_%s_%d_x_%d_config,
                        .lsc_gain_table_size = %d
                    }
                };'''%(name, lsc.img_w, lsc.img_h, name, lsc.img_w, lsc.img_h, len(lsc.table))
            )

            return lsc_text

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
            acc_text += ccm_code(name, obj)
            acc_obj_text += ('''
                .ccm = &s_esp_ipa_acc_ccm_%s_config,
                '''%(name)
            )
        
        if hasattr(obj, 'lsc'):
            acc_text += lsc_code(name, obj)
            acc_obj_text += ('''
                .lsc_table = s_esp_ipa_acc_lsc_%s_config,
                .lsc_table_size = ARRAY_SIZE(s_esp_ipa_acc_lsc_%s_config),
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
            gamma = obj.gamma
            gamma_table_text = str()
            use_gamma_param_str = str()

            gamma_text = str()
            if gamma.use_gamma_param == False:
                gamma_table_str = str()
                use_gamma_param_str = 'false'

                for u in gamma.table:
                    gamma_x = str()
                    for i in range(0, 16): gamma_x += '%3d, ' %(min((i + 1) * 16, 255))
                    
                    gamma_y = str()
                    for i in u.y: gamma_y += '%3d, '%(i)

                    gamma_table_str += cfmt_string('''
                        {
                            .luma = %0.4f,
                            .gamma = {
                                .x = {
                                    %s
                                },
                                .y = {
                                    %s
                                }
                            }
                        },
                        '''%(u.luma, gamma_x, gamma_y)
                    )
                
                gamma_table_text = cfmt_string('''
                    static const esp_ipa_aen_gamma_unit_t s_esp_ipa_aen_gamma_%s_table[] = {
                        %s
                    };
                    '''%(name, gamma_table_str)
                )
            else:
                gamma_table_str = str()
                use_gamma_param_str = 'true'

                for u in gamma.table:
                    gamma_table_str += cfmt_string('''
                        {
                            .luma = %0.4f,
                            .gamma_param = %0.4f,
                        },
                        '''%(u.luma, u.gamma_param)
                    )
                
                gamma_table_text = cfmt_string('''
                    static const esp_ipa_aen_gamma_unit_t s_esp_ipa_aen_gamma_%s_table[] = {
                        %s
                    };
                    '''%(name, gamma_table_str)
                )

            if not hasattr(gamma, 'model'):
                gamma.model = 0

            gamma_text = cfmt_string('''
                .model = %d,
                .use_gamma_param = %s,
                .luma_env = \"%s\",
                .luma_min_step = %0.4f,
                .gamma_table = s_esp_ipa_aen_gamma_%s_table,
                .gamma_table_size = %d,
                '''%(gamma.model, use_gamma_param_str, gamma.luma_env, gamma.luma_min_step, name, len(gamma.table))
            )

            return (gamma_text, gamma_table_text)

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
                contrast_text += cfmt_string('''
                    {
                        .gain = %d,
                        .contrast = %d
                    },'''%(i.gain * 1000, i.value)
                )
            return contrast_text

        aen_code = str()
        aen_obj_code = str()
        if hasattr(obj, 'gamma'):
            text = gamma_code(name, obj)
            aen_code += text[1]
            aen_code += cfmt_string('''
                static const esp_ipa_aen_gamma_config_t s_ipa_aen_gamma_%s_config = {
                    %s
                };'''%(name, text[0])
            )
            aen_obj_code += cfmt_string('''
                .gamma = &s_ipa_aen_gamma_%s_config,
                '''%(name)
            )

        if hasattr(obj, 'sharpen'):
            aen_code += cfmt_string('''
                static const esp_ipa_aen_sharpen_t s_ipa_aen_sharpen_%s_config[] = {
                    %s
                };'''%(name, sharpen_code(name, obj))
            )
            aen_obj_code += cfmt_string('''
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
            aen_obj_code += cfmt_string('''
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
                m = list()
                if hasattr(p, 'matrix'):
                    m = p.matrix
                elif hasattr(p, 'sigma'):
                    sigma = p.sigma
                    num = 3
                    for x in range(0, num):
                       for y in range(0, num):
                            _x = y - (num // 2)
                            _y = (num // 2) - x
                            k = math.exp(-(_x ** 2 + _y ** 2) / (2 * sigma ** 2)) / (2 * math.pi * sigma ** 2)
                            m.append(k)
                    k_max = max(m)
                    for n in range(0, num ** 2):
                        m[n] = min(math.floor(m[n] / k_max * 15 + 0.5), 15)
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

        def get_anti_flicker_param(obj):
            mode_desc_dic = {
                'full': { 'mode': 'ESP_IPA_AGC_ANTI_FLICKER_FULL', 'ac_freq': True },
                'part': { 'mode': 'ESP_IPA_AGC_ANTI_FLICKER_PART', 'ac_freq': True },
                'none': { 'mode': 'ESP_IPA_AGC_ANTI_FLICKER_NONE', 'ac_freq': False },
            }
            ac_freq = 0
            agc_af = obj.anti_flicker

            mode_desc = mode_desc_dic[agc_af.mode]['mode']
            if mode_desc_dic[agc_af.mode]['ac_freq']:
                ac_freq = agc_af.ac_freq
            
            return (mode_desc, ac_freq)

        def exposure_code(name, obj):
            if not hasattr(obj, 'f_m0'):
                obj.f_m0 = obj.f_n0

            exp = obj.exposure
            gain = obj.gain
            anti_flicker_mode, af_freq = get_anti_flicker_param(obj)
            exposure_text = ('''
                .exposure_frame_delay = %d,
                .exposure_adjust_delay = %d,
                .gain_frame_delay = %d,
                .min_gain_step = %0.4f,
                .inc_gain_ratio = %0.4f,
                .dec_gain_ratio = %0.4f,
                .anti_flicker_mode = %s,
                .ac_freq = %d,
                '''%(exp.frame_delay, exp.adjust_delay,
                     gain.frame_delay,gain.min_step, obj.f_n0, obj.f_m0,
                     anti_flicker_mode, af_freq)
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

class ipa_unit_customized_c(ipa_unit_c):
    def customized_name(self):
        return self.name

    def decode(self, obj):
        self.name = obj.name

class ipa_unit_atc_c(ipa_unit_c):
    @staticmethod
    def decode_atc(name, obj):
        if not hasattr(obj, 'model'):
            obj.model = 0

        def luma_lut_code(name, obj):
            luma_lut_text = str()
            for i in obj.luma_lut:
                luma_lut_text += ('''
                    {
                        .luma = %f,
                        .ae_value = %d
                    },'''%(i.luma, i.ae_value)
                )
            return luma_lut_text

        atc_code = str()
        atc_obj_code = str()
        if hasattr(obj, 'luma_lut'):
            atc_code += cfmt_string('''
                static const esp_ipa_atc_luma_lut_t s_ipa_atc_luma_lut_%s_config[] = {
                    %s
                };'''%(name, luma_lut_code(name, obj))
            )

        atc_code += cfmt_string('''
            static const esp_ipa_atc_config_t s_ipa_atc_%s_config = {
                .model = %d,
                .init_value = %d,
                .delay_frames = %d,
                .luma_env = \"%s\",
                .min_ae_value_step = %d,
                .luma_lut = s_ipa_atc_luma_lut_%s_config,
                .luma_lut_size = ARRAY_SIZE(s_ipa_atc_luma_lut_%s_config)
            };
            '''%(name, obj.model, obj.init_value, obj.delay_frames, obj.luma_env, obj.min_ae_value_step,
                 name, name)
        )

        return atc_code

    def decode(self, obj):
        self.text = self.decode_atc(self.name, obj)

class ipa_unit_ext_c(ipa_unit_c):
    @staticmethod
    def decode_ext(name, obj):
        ext = ext_default(name)
        if hasattr(obj, 'hue'):
            ext.hue = obj.hue
        if hasattr(obj, 'brightness'):
            ext.brightness = obj.brightness
        if hasattr(obj, 'stats_region'):
            ext.stats_region = obj.stats_region

        ext_code = cfmt_string('''
            static const esp_ipa_ext_config_t s_ipa_ext_%s_config = {
                .hue = %d,
                .brightness = %d,
                .stats_region = {
                    .top = %d,
                    .left = %d,
                    .width = %d,
                    .height = %d
                }
            };
            '''%(name, obj.hue, obj.brightness, obj.stats_region.top,
                 obj.stats_region.left, obj.stats_region.width,
                 obj.stats_region.height)
        )

        return ext_code

    def decode(self, obj):
        self.text = self.decode_ext(self.name, obj)

class ipa_unit_af_c(ipa_unit_c):
    @staticmethod
    def decode_af(name, obj):
        class af_window():
            def __init__(self, left, top, width, height, weight):
                self.top_left_x = left
                self.top_left_y = top
                self.btm_right_x = left + width - 1
                self.btm_right_y = top + height - 1
                self.weight = weight
        if not hasattr(obj, 'max_pos'):
            obj.max_pos = 0
        if not hasattr(obj, 'min_pos'):
            obj.min_pos = 0
        if not hasattr(obj, 'l1_scan_points_num'):
            obj.l1_scan_points_num = 10
        if not hasattr(obj, 'l2_scan_points_num'):
            obj.l2_scan_points_num = 10
        if not hasattr(obj, 'definition_high_threshold_ratio'):
            obj.definition_high_threshold_ratio = 1.5
        if not hasattr(obj, 'definition_low_threshold_ratio'):
            obj.definition_low_threshold_ratio = 0.5
        if not hasattr(obj, 'luminance_high_threshold_ratio'):
            obj.luminance_high_threshold_ratio = 1.5
        if not hasattr(obj, 'luminance_low_threshold_ratio'):
            obj.luminance_low_threshold_ratio = 0.5
        if not hasattr(obj, 'max_change_time'):
            obj.max_change_time = 1000000

        af_windows = list()
        if not hasattr(obj, 'windows'):
            fatal_error('AF has no windows')
        else:
            if len(obj.windows) > 3 or len(obj.windows) == 0:
                fatal_error('AF windows number must <= 3 and not 0')
            else:
                for w in obj.windows:
                    if not hasattr(w, 'weight'):
                        w.weight = 1
                    af_windows.append(af_window(w.left, w.top, w.width, w.height, w.weight))

                null_windows = 3 - len(obj.windows)
                for i in range(0, null_windows):
                    af_windows.append(af_window(2, 2, 4, 4, 0))

        af_code = cfmt_string('''
            static const esp_ipa_af_config_t s_ipa_af_%s_config = {
                .windows = {
                    [0] = {
                        .top_left = {
                            .x = %d,
                            .y = %d,
                        },
                        .btm_right = {
                            .x = %d,
                            .y = %d,
                        },
                    },
                    [1] = {
                        .top_left = {
                            .x = %d,
                            .y = %d,
                        },
                        .btm_right = {
                            .x = %d,
                            .y = %d,
                        },
                    },
                    [2] = {
                        .top_left = {
                            .x = %d,
                            .y = %d,
                        },
                        .btm_right = {
                            .x = %d,
                            .y = %d,
                        },
                    },
                },
                .weight_table = {
                    %d, %d, %d
                },
                .edge_thresh = %d,
                .l1_scan_points_num = %d,
                .l2_scan_points_num = %d,
                .max_pos = %d,
                .min_pos = %d,
                .definition_high_threshold_ratio = %0.4f,
                .definition_low_threshold_ratio = %0.4f,
                .luminance_high_threshold_ratio = %0.4f,
                .luminance_low_threshold_ratio = %0.4f,
                .max_change_time = %d
            };
            '''%(name, af_windows[0].top_left_x, af_windows[0].top_left_y,
                 af_windows[0].btm_right_x, af_windows[0].btm_right_y,
                 af_windows[1].top_left_x, af_windows[1].top_left_y,
                 af_windows[1].btm_right_x, af_windows[1].btm_right_y,
                 af_windows[2].top_left_x, af_windows[2].top_left_y,
                 af_windows[2].btm_right_x, af_windows[2].btm_right_y,
                 af_windows[0].weight, af_windows[1].weight, af_windows[2].weight,
                 obj.edge_thresh, obj.l1_scan_points_num, obj.l2_scan_points_num,
                 obj.max_pos, obj.min_pos,
                 obj.definition_high_threshold_ratio, obj.definition_low_threshold_ratio,
                 obj.luminance_high_threshold_ratio, obj.luminance_low_threshold_ratio,
                 obj.max_change_time)
        )

        return af_code

    def decode(self, obj):
        self.text = self.decode_af(self.name, obj)

class ipa_c(object):
    def __init__(self, name, version):
        self.name = name
        self.decodes = list()
        self.version = version

    def add(self, obj, name, type):
        ipa_unit_lut = {
            'ian': ipa_unit_ian_c,
            'awb': ipa_unit_awb_c,
            'acc': ipa_unit_acc_c,
            'aen': ipa_unit_aen_c,
            'adn': ipa_unit_adn_c,
            'agc': ipa_unit_agc_c,
            'atc': ipa_unit_atc_c,
            'ext': ipa_unit_ext_c,
            'af':  ipa_unit_af_c
        }

        if type in ipa_unit_lut:
            self.decodes.append(ipa_unit_lut[type](obj, name, type))
        else:
            if 'customized_ipa_' in type:
                self.decodes.append(ipa_unit_customized_c(obj, name, type))

    def get_text(self):        
        text_obj = str()
        text = str()
        names = str()

        for i in self.decodes:
            cname = i.customized_name()
            if cname == None:
                text += i.get_text()
                text_obj += ('.%s = &s_ipa_%s_%s_config,\n'%(i.type, i.type, i.name))
                names += ('\"esp_ipa_%s\",\n'%(i.type))
            else:
                names += ('\"%s\",\n'%(cname))

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
