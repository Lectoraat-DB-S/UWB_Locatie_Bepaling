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
  "y": 650,
  "z": 600
}

point4 = {
  "type": "R",
  "x": 550,
  "y": 800,
  "z": 600
}
rpoint=point
rpoint3=point3
rpoint["x"] = point["x"]
rpoint["y"] = point["y"]
rpoint3["x"] = point3["x"]
rpoint3["y"] = point3["y"]

for i in range(-50,50,1):
    point["x"] = rpoint["x"] + (i *2) % 20 
    point["y"] = rpoint["y"] + (i *2) % 30
    point3["x"] = rpoint3["x"] + (i *2) % 10 
    point3["y"] = rpoint3["y"] + (i *2) % 15
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

print("finished loop")

