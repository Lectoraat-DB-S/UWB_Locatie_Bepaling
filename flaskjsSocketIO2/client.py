import requests
url = 'http://localhost:5000/api/data'
json_data = {
    'Name': 'VD',
    'Job':'Dev',
}
response = requests.post(url, json=json_data)
if response.status_code == 200:
    print('JSON data sent successfully!')
else:
    print('Failed to send JSON data:', response.status_code)