function updateLine(e) {
    let r = canvas.getBoundingClientRect(),
        x = e.clientX - r.left,
        y = e.clientY - r.top;

    socket.emit('update_event', {"message": "getdata from server 1"});

    socket.once('message', (data) => {
        UWBdata = data;
    });

    xprev = x;
    yprev = y;
    startdraw(x, y);
}

function updateLine2(e) {
    socket.emit('update_event', {"message": "getdata from server 2"});

    if (e && 'tag_id' in e) {
        displayTagLocation(e);
    }

    socket.once("message", (data) => {
        UWBdata = data;
    });

    startdraw(xprev, yprev);
}

function displayTagLocation(data) {
    let tagId = data.tag_id;
    let position = calculateTagPosition(data);

    if (position.x === -1 && position.y === -1) {
        // logOnLoggingPanel(`Invalid position for tag ${tagId}`);
        return;
    }

    // add new tag position to tagPositions
    tagPositions[tagId] = {
        x: position.x,
        y: position.y
    };

    // Update the timestamp for this tag
    lastDrawnTagTimes[tagId] = Date.now();

    startdraw(xprev, yprev);
}
