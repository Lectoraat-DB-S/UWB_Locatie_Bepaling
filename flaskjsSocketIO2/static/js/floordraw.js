var canvas = document.getElementById('myCanvas'), /// canvas element
    ctx = canvas.getContext('2d'),            /// context
    line = new Line(ctx),                     /// our custom line object
    img = new Image;                          /// the image for bg

ctx.strokeStyle = '#f0f';                     /// white line for demo

/// start image loading, when done draw and setup 
img.onload = start;
img.src = "static/images/UWBprototypeFvT.png";
var hRatio = canvas.width / img.width ;
var vRatio = canvas.height / img.height ;
var ratio = Math.min ( hRatio, vRatio );

var socket = io.connect('http://localhost:5000/');
/// Sen JSON data
data = {"message": "Hello, server!"};
socket.emit('my_event', data);
console.log('hello server from java')

/// Disconnect from the server
/// sio.disconnect()
socket.on("message", (data) => {
    console.log('message from flask server received:', data);
  })

canvas.addEventListener('UWBdata', (event) => {
    console.log('update canvas!');
    updateLine;});


function PointU(type, x,y,z){
 this.type = type;
 this.x = x ;
 this.y = y;
 this.z = z;
}



// const data1 = [new PointU("M", 100,100,50), new PointU("M",150,150,10), new PointU("M",200.200,50)]

xprev =-1
yprev = -1
/// ctx.drawImage(img, 0,0, img.width, img.height, 0,0,img.width*ratio, img.height*ratio);
function start() {
    /// initial draw of image
    console.log("Start drawing")
    ctx.drawImage(img, 0, 0,canvas.width, canvas.height);
    startdraw(xprev,yprev)
    /// listen to mouse move (or use jQuery on('mousemove') instead)
   
    
    canvas.onclick= updateLine;
    socket.on("UWBdata",updateLine2);
   

    
}
/// updates the line on each mouse move 
   
function updateLine(e) {

    /// correct mouse position so it's relative to canvas
     var r = canvas.getBoundingClientRect(),
        x = e.clientX - r.left,
        y = e.clientY - r.top;
    
    console.log("mouse position "+ x + ":" + y);
    data = {"message": "getdata from server 1"};
    socket.emit('update_event', data);

    socket.on('message', (data) => {
   
    console.log('JSON message received in 1:', data);
    UWBdata = data;
    });
    xprev =x;
    yprev =y;
    startdraw(x,y);
}

function updateLine2(e) {

    /// correct mouse position so it's relative to canvas
    data = {"message": "getdata from server 2"};
    socket.emit('update_event', data);

    socket.on("message", (data) => { 

    /** socket.on("UWBdata", (data) => {**/
    console.log('JSON message received in 2:', data);
    UWBdata = data;
    }); 

    startdraw(xprev,yprev);
}

function updateLine3(e) {

    /// correct mouse position so it's relative to canvas
 
    socket.on("UWBdata", (data) => {
    console.log('JSON message received in 2:', data);
    UWBdata = data;
    }); 
    startdraw(xprev,yprev);
}

function startdraw(x,y){
    /// draw background image to clear previous line
    ctx.drawImage(img, 0, 0, canvas.width, canvas.height);
    var data1 = UWBdata;
    console.log("type : ", typeof(data1))
    console.log("transfer: " + JSON.stringify(data1) )
    var delta = 0;
    for(let i=0;i < data1.length;i++){
    	var point = data1[i]
        if (delta > 0){delta = 0; console.log("delta1")} else {delta = 30; console.log("delta0")}
	
    	console.log("point: (" + (point.x) + "," + point.y + ")")
       	console.log("point : " + JSON.stringify(point))
         line.x1 = point.x;
    
    line.y1 = point.y-10;
    line.x2 = point.x;
    // line.y2 = canvas.height;
    line.y2 =  point.y+10
    line.draw();
    line.x1 = point.x-10;
    line.y1 = point.y;
    line.x2 = point.x+10;
    line.y2 = point.y;
    line.draw(); 
    
    if (xprev >= 0){
    	line.x1 = x ;
    	line.y1 = y;
    	line.x2 = point.x;
    // line.y2 = canvas.height;
    	line.y2 = point.y
    	line.draw(); }

    	ctx.fillStyle = 'blue';
    	ctx.font = '20px Arial';
    	ctx.fillText('UWB-' + i + "(" + point.x + " " + point.y +" "+point.type+")", point.x,point.y+20);
       }
    
	
    /// console.log("point: (" + data1.x + "," + data1.y + ")")
    /// var point = data1.pop();
    /// console.log("point : " + JSON.stringify(point))

    /// update line object and draw it
    if (xprev >0){
    line.x1 = x;
    line.y1 = y-10;
    line.x2 = x;
    // line.y2 = canvas.height;
    line.y2 = y+10
    line.draw();
    line.x1 = x-10;
    line.y1 = y;
    line.x2 = x+10;
    line.y2 = y;
    line.draw();

    
      // Drawing on top of the image
        ctx.fillStyle = 'red';
        ctx.font = '20px Arial';
        ctx.fillText(("(" + Math.round(x) + "," + Math.round(y) +")"),x,y+30);
    }
}

/// This lets us define a custom line object which self-draws
function Line(ctx) {

    var me = this;

    this.x1 = 0;
    this.x2 = 0;
    this.y1 = 0;
    this.y2 = 0;

    /// call this method to update line        
    this.draw = function() {
        ctx.beginPath();
        ctx.moveTo(me.x1, me.y1);
        ctx.lineTo(me.x2, me.y2);
        ctx.stroke();
    }
}
