from flask import Flask, request
 
app = Flask(__name__)
 
@app.route('/api/data', methods=['POST'])  
def api_data():
    if request.method == 'POST':
        data = request.json 
        print('Received JSON data:', data)
        return 'JSON data received successfully!', 200
    else:
        return 'Method Not Allowed', 405
 
if __name__ == '__main__':
    app.run(debug=True)