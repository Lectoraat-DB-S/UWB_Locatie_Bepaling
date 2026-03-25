let lastDrawnTagPositions = {};
let lastDrawnTagTimes = {};

function Line(ctx) {
    this.x1 = 0;
    this.x2 = 0;
    this.y1 = 0;
    this.y2 = 0;

    this.draw = function() {
        ctx.beginPath();
        ctx.moveTo(this.x1, this.y1);
        ctx.lineTo(this.x2, this.y2);
        ctx.stroke();
    };
}

function startdraw(x, y) {
    ctx.drawImage(img, 0, 0, canvas.width, canvas.height);
    let data1 = typeof UWBdata !== 'undefined' ? UWBdata : [];

    let delta = 0;
    for(let i = 0; i < data1.length; i++) {
        let point = data1[i];
        delta = delta > 0 ? 0 : 30;

        // Draw vertical line at point
        line.x1 = point.x;
        line.y1 = point.y - 10;
        line.x2 = point.x;
        line.y2 = point.y + 10;
        line.draw();

        // Draw horizontal line at point
        line.x1 = point.x - 10;
        line.y1 = point.y;
        line.x2 = point.x + 10;
        line.y2 = point.y;
        line.draw();

        // Draw line from click to point
        if (xprev >= 0) {
            line.x1 = x;
            line.y1 = y;
            line.x2 = point.x;
            line.y2 = point.y;
            line.draw();
        }

        // Draw label for UWB point
        ctx.fillStyle = 'blue';
        ctx.font = '20px Arial';
        ctx.fillText('UWB-' + i + "(" + point.x + " " + point.y + " " + point.type + ")", point.x, point.y + 20);
    }

    // Draw crosshair at click position
    if (xprev > 0) {
        // Draw vertical line
        line.x1 = x;
        line.y1 = y - 10;
        line.x2 = x;
        line.y2 = y + 10;
        line.draw();

        // Draw horizontal line
        line.x1 = x - 10;
        line.y1 = y;
        line.x2 = x + 10;
        line.y2 = y;
        line.draw();

        x = Math.round(x);
        y = Math.round(y);

        // Draw coordinates
        ctx.fillStyle = 'red';
        ctx.font = '20px Arial';
        ctx.fillText("(" + x + "," + y + ")", x, y + 30);

        // Update selected point in data pane
        updateCurrentSelectedPoint(x, y);
    }

    drawAnchors();
    drawTags();
}

function drawAnchors() {
    let anchorData = anchorPositions;
    if (anchorPositions && anchorPositions.data && typeof anchorPositions.data === 'object') {
        anchorData = anchorPositions.data;
    }

    // Draw all assigned anchors
    for (let mac in anchorData) {
        let pos = anchorData[mac];

        // Draw anchor symbol (larger cross)
        ctx.strokeStyle = '#0000FF'; // Green color for anchors
        ctx.lineWidth = 3;

        // Draw anchor cross
        ctx.beginPath();
        ctx.moveTo(pos.x - 15, pos.y);
        ctx.lineTo(pos.x + 15, pos.y);
        ctx.stroke();

        ctx.beginPath();
        ctx.moveTo(pos.x, pos.y - 15);
        ctx.lineTo(pos.x, pos.y + 15);
        ctx.stroke();

        // Draw anchor label
        ctx.fillStyle = '#00aa00';
        ctx.font = 'bold 16px Arial';
        ctx.fillText('Anchor: ' + mac, pos.x + 20, pos.y);

        // Reset styles
        ctx.strokeStyle = '#f0f';
        ctx.lineWidth = 1;
    }
}

function drawTags() {
    const currentTime = Date.now();
    const staleThreshold = 7500; // 3 seconds in milliseconds
    
    for (let mac in tagPositions) {
        let pos = tagPositions[mac];
        let x = pos.x;
        let y = pos.y;
        let newColor = '#FF0000'; // Red color for default, out of bounds
        
        // Check if tag data is stale (older than threshold)
        // If we haven't seen this tag before, initialize its timestamp
        if (!lastDrawnTagTimes[mac]) {
            lastDrawnTagTimes[mac] = currentTime;
        }
        
        const timeSinceLastUpdate = currentTime - lastDrawnTagTimes[mac];
        const isStale = timeSinceLastUpdate > staleThreshold;
        
        // Skip drawing stale tags
        if (isStale) {
            console.log(`Tag ${mac} has stale data (${Math.round(timeSinceLastUpdate/1000)}s old). Not drawing.`);
            continue;
        }
        
        // Check if position is out of bounds
        let isOutOfBounds = (x < 0 || y < 0 || x > canvas.width || y > canvas.height);
        
        // If out of bounds and we have a last known position, use that instead
        if (isOutOfBounds && lastDrawnTagPositions[mac]) {
            x = lastDrawnTagPositions[mac].x;
            y = lastDrawnTagPositions[mac].y;
        } 
        // If still out of bounds (no previous valid position), skip drawing
        else if (isOutOfBounds) {
            continue;
        }
        // If position is valid, update lastDrawnTagPositions
        else {
            lastDrawnTagPositions[mac] = { x: x, y: y };
            newColor = '#00FF00'; // Green color for valid positions
        }

        // Draw tag symbol (smaller cross)
        ctx.strokeStyle = newColor;
        ctx.lineWidth = 2;

        // Draw tag cross
        ctx.beginPath();
        ctx.moveTo(x - 10, y);
        ctx.lineTo(x + 10, y);
        ctx.stroke();

        ctx.beginPath();
        ctx.moveTo(x, y - 10);
        ctx.lineTo(x, y + 10);
        ctx.stroke();

        // Draw tag label
        ctx.fillStyle = newColor;
        ctx.font = 'bold 14px Arial';
        ctx.fillText('Tag: ' + mac, x + 15, y);

        // Reset styles
        ctx.strokeStyle = '#f0f';
        ctx.lineWidth = 1;
    }
}

function resizeCanvasToImage(image) {
    // Resize the canvas to match the image dimensions
    // while strictly preserving aspect ratio
    const maxWidth = 1600; // Maximum width for canvas
    const maxHeight = 1200; // Maximum height for canvas

    // Get original dimensions and aspect ratio
    const originalWidth = image.width;
    const originalHeight = image.height;
    const aspectRatio = originalWidth / originalHeight;

    // Start with original dimensions
    let newWidth = originalWidth;
    let newHeight = originalHeight;

    // Scale down if needed, preserving aspect ratio
    if (newWidth > maxWidth || newHeight > maxHeight) {
        // Calculate scaling factors for both constraints
        const widthRatio = maxWidth / originalWidth;
        const heightRatio = maxHeight / originalHeight;

        // Use the smaller ratio to ensure image fits within both constraints
        const scaleFactor = Math.min(widthRatio, heightRatio);

        // Apply the scale factor to both dimensions to maintain aspect ratio
        newWidth = Math.floor(originalWidth * scaleFactor);
        newHeight = Math.floor(originalHeight * scaleFactor);
    }

    // Set canvas dimensions
    canvas.width = newWidth;
    canvas.height = newHeight;

    logOnLoggingPanel(`Canvas resized to ${newWidth}x${newHeight} (aspect ratio: ${aspectRatio.toFixed(2)})`);
}
