function syncServerAnchorPositions(http_method) {
    if (http_method === 'PUT') {
        // Use consistent data structure when sending
        socket.emit('sync_anchor_positions', anchorPositions);
    } else if (http_method === 'GET') {
        socket.emit('get_anchor_positions', {});
        return new Promise((resolve, reject) => {
            socket.once('anchor_positions', (data) => {
                // Store the anchors directly without data wrapper
                anchorPositions = data.data || {};
                resolve(anchorPositions);
            });
        });
    } else if (http_method === 'DELETE') {
        anchorPositions = {}; // Clear local cache
        socket.emit('delete_anchor_positions', {});
    }

    return Promise.resolve(true);
}

function get_anchors_and_refresh_canvas() {
    // Get and apply anchor positions from server
    syncServerAnchorPositions('GET').then(() => {
        ctx.drawImage(img, 0, 0, canvas.width, canvas.height);
        startdraw(xprev, yprev);
        console.log('Image loaded and canvas drawn.');
    }).catch(error => {
      console.error('Error in syncServerAnchorPositions:', error);
    });
    // .then() makes sure that the anchor positions are loaded before drawing
}
