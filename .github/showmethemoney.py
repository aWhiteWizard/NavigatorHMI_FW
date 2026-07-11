'''
Author: aWhiteWizard www.123518341@qq.com
Date: 2026-07-11 09:03:11
LastEditors: aWhiteWizard www.123518341@qq.com
LastEditTime: 2026-07-11 09:46:17
FilePath: /NavigatorHMI_FW/.github/showmethemoney.py
Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
'''
import requests

url = "https://api.deepseek.com/user/balance"

payload={}
headers = {
  'Accept': 'application/json',
  'Authorization': 'Bearer sk-6a525ee70ee54625ab81ec78ca5b0d20'
}

response = requests.request("GET", url, headers=headers, data=payload)

print(response.text)