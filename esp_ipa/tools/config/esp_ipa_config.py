# SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

import argparse
import sys
import json
import os

sys.path.append(os.path.dirname(os.path.abspath(__file__)) + '/isp')

import customized, agc, atc, af, ext, acc, aen, adn, ian, awb, common

class ipa_c(object):
    def __init__(self, name, version):
        self.name = name
        self.decodes = list()
        self.version = version

    def add(self, obj, name, type):
        ipa_unit_lut = {
            'ian': ian.ipa_unit_ian_c,
            'awb': awb.ipa_unit_awb_c,
            'acc': acc.ipa_unit_acc_c,
            'aen': aen.ipa_unit_aen_c,
            'adn': adn.ipa_unit_adn_c,
            'agc': agc.ipa_unit_agc_c,
            'atc': atc.ipa_unit_atc_c,
            'ext': ext.ipa_unit_ext_c,
            'af':  af.ipa_unit_af_c
        }

        if type in ipa_unit_lut:
            self.decodes.append(ipa_unit_lut[type](obj, name, type))
        else:
            if 'customized_ipa_' in type:
                self.decodes.append(customized.ipa_unit_customized_c(obj, name, type))

    def get_text(self):        
        text_obj = str()
        text = str()
        names = str()

        for i in self.decodes:
            cname = i.customized_name()
            if cname == None:
                text += i.get_text()
                text_obj += (f'.{i.type} = &s_ipa_{i.type}_{i.name}_config,\n')
                names += (f'\"esp_ipa_{i.type}\",\n')
            else:
                names += (f'\"{cname}\",\n')

        text += common.cfmt_string(f'''
            static const char *s_ipa_{self.name}_names[] = {{
                {names}
            }};'''
        )

        text += common.cfmt_string(f'''
            static const esp_ipa_config_t s_ipa_{self.name}_config = {{
                .names = s_ipa_{self.name}_names,
                .nums = ARRAY_SIZE(s_ipa_{self.name}_names),
                .version = {self.version},
                {text_obj}
            }};'''
        )

        return text

class ipas_c(object):
    def __init__(self):
        self.ipa_list = list()
        self.license = common.cfmt_string(f'''
            /*
             * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
             *
             * SPDX-License-Identifier: ESPRESSIF MIT
             */''')

        self.header = common.cfmt_string(f'''
            #include <string.h>
            #include "esp_ipa.h"
            ''')

        self.typedef = common.cfmt_string(f'''
            typedef struct esp_video_ipa_index {{
                const char *name;
                const esp_ipa_config_t *ipa_config;
            }} esp_video_ipa_index_t;
            ''')

        self.get_config_func = common.cfmt_string(f'''
            const esp_ipa_config_t *esp_ipa_pipeline_get_config(const char *name)
            {{
                for (int i = 0; i < ARRAY_SIZE(s_video_ipa_configs); i++) {{
                    if (!strcmp(name, s_video_ipa_configs[i].name)) {{
                        return s_video_ipa_configs[i].ipa_config;
                    }}
                }}

                return NULL;
            }}''')

        self.get_null_config_func = common.cfmt_string(f'''
            const esp_ipa_config_t *esp_video_isp_pipeline_get_ipa_config(const char *name)
            {{
                return NULL;
            }}''')

    def get_config(self):
        text = str()
        if len(self.ipa_list) > 0:
            def ipa_all_text(ipas):
                ipas_text = str()
                for i in ipas:
                    ipas_text += i.get_text()
                return ipas_text

            def ipa_table_code(ipas):
                ipa_table_text = str()
                for i in ipas:
                    ipa_table_text += (f'''
                        {{
                            .name = \"{i.name}\",
                            .ipa_config = &s_ipa_{i.name}_config
                        }},'''
                    )
                return ipa_table_text

            text = ipa_all_text(self.ipa_list)
            text += common.cfmt_string(f'''
                static const esp_video_ipa_index_t s_video_ipa_configs[] = {{
                    {ipa_table_code(self.ipa_list)}
                }};''')

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
                        raise common.fatal_error(f'IPA json configuration file version should be {version}')
                else:
                    ipa = ipa_c(k, version)
                    for i in j[k]: ipa.add(common.dict_object(j[k][i]), k, i)
                    ipas.add(ipa)

        input_info = common.cfmt_string(f'''
            /* Json file: {input} */
            ''')

        with open(output, 'w') as fp:
            fp.write(ipas.get_text() + input_info)
    else:
        text = common.cfmt_string(f'''
            const void *esp_ipa_pipeline_get_config(const char *name)
            {{
                return NULL;
            }}''')

        with open(output, 'w') as fp:
            fp.write(text)

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
    except common.fatal_error as e:
        print(f'\nA fatal error occurred: {e}')
        sys.exit(2)

if __name__ == '__main__':
    _main()
