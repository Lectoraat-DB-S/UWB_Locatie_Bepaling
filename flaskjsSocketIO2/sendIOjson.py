import requests
import json

#  data = [100,200,300,300,300,400]

# a Python object (dict):
point = {
  "type": "T",
  "x": 300,
  "y": 250,
  "z": 400
}

point2 = {
  "type": "T",
  "x": 400,
  "y": 550,
  "z": 400
}

point3 = {
  "type": "R",
  "x": 500,
  "y": 600,
  "z": 600
}

point4 = {
  "type": "R",
  "x": 550,
  "y": 800,
  "z": 600
}

points=list((point,point2,point3,point4))
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



