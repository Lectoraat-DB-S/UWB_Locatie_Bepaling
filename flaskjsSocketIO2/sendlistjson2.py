import requests
import json

#  data = [100,200,300,300,300,400]

# a Python object (dict):
point = {
  "type": "X",
  "x": 300,
  "y": 300,
  "z": 400
}

point2 = {
  "type": "Y",
  "x": 400,
  "y": 500,
  "z": 400
}

point3 = {
  "type": "Z",
  "x": 400,
  "y": 400,
  "z": 600
}

points=list((point,point2,point3))
# points=point
# the result is an array of JSON string:
print(points)



url = 'http://localhost:5000/api/data'
json_data = points

response = requests.post(url, json=json_data)
if response.status_code == 200:
    print('JSON data sent successfully!')
else:
    print('Failed to send JSON data:', response.status_code)

point3["type"] ="A"
points=list((point,point2,point3))
json_data = points
print(points)
response = requests.post(url, json=json_data)
if response.status_code == 200:
    print('JSON data sent successfully!')
else:
    print('Failed to send JSON data:', response.status_code)

