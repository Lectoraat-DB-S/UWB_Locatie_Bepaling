let canvas = document.getElementById('myCanvas');
let ctx = canvas.getContext('2d');
let line = new Line(ctx);
let img = new Image;

ctx.strokeStyle = '#f0f';

let socket = io.connect(window.location.origin);
let xprev = -1;
let yprev = -1;

let anchorMacs = [];
let anchorPositions = {};
let tagPositions = {};

img.onload = start;
img.src = "static/images/UWBprototypeFvT.png";

let distanceScale = 0.5; // Scaling factor to convert cm to pixel

function start() {
    get_anchors_and_refresh_canvas();

    canvas.onclick = updateLine;
    socket.on("UWBdata", updateLine2);

    let positionAssignButton = document.getElementById('assign-anchor');
    positionAssignButton.onclick = function() {
        let selectedPointP = document.getElementById('selected-point');
        let positionDropdown = document.getElementById('anchor-dropdown');

        let selectedMac = positionDropdown.value;
        let x = parseInt(selectedPointP.getAttribute('m-pos-x'));
        let y = parseInt(selectedPointP.getAttribute('m-pos-y'));
        let z = prompt('Please enter the z-coordinate (height) for the anchor:', '0');
        z = parseInt(z);

        if (selectedMac && selectedMac.toLowerCase() !== 'none' && x && y && z) {
            z *= distanceScale; // Convert cm to pixel using current scale
            anchorPositions[selectedMac] = {
                x: x,
                y: y,
                z: z
            };

            logOnLoggingPanel("Assigned " + selectedMac + " to (" + x + ", " + y + ")");

            // Sync anchor positions with server
            syncServerAnchorPositions('PUT').then();

            get_anchors_and_refresh_canvas();

        } else {
            alert("Please select an anchor and click on the map to assign a position.");
        }
    }

    let positionResetButton = document.getElementById('reset-anchor');
    positionResetButton.onclick = function() {
        let positionDropdown = document.getElementById('anchor-dropdown');
        let selectedMac = positionDropdown.value;

        if (selectedMac && selectedMac.toLowerCase() !== 'none') {
            delete anchorPositions[selectedMac];
            logOnLoggingPanel("Removed anchor " + selectedMac);

            // Redraw the canvas to show the removed anchor
            startdraw(xprev, yprev);

            // Sync anchor positions with server
            syncServerAnchorPositions('PUT').then();
        } else {
            alert("Please select an anchor to remove.");
        }
    }

    let positionResetAllButton = document.getElementById('reset-all-anchors');
    positionResetAllButton.onclick = function() {
        anchorPositions = {};
        logOnLoggingPanel("Removed all anchors");

        // Redraw the canvas to show the removed anchors
        startdraw(xprev, yprev);

        // Sync anchor positions with server
        syncServerAnchorPositions('DELETE').then();
    }

    // setupConfigButtons();
    setupImageCustomization(); // Add this line to setup image customization

    socket.on('anchormacs', (data) => {

        // compare data with anchorMacs
        let newMacs = data.data.filter(mac => !anchorMacs.includes(mac));

        // add new macs to anchorMacs
        anchorMacs = anchorMacs.concat(newMacs);

        if (newMacs.length > 0) {
            let select_option_group = document.getElementById('anchor-dropdown-opts');

            for (let i = 0; i < newMacs.length; i++) {
                let opt = document.createElement('option');
                opt.value = newMacs[i];
                opt.innerHTML = newMacs[i];
                select_option_group.appendChild(opt);
            }
        }
    });

    logOnLoggingPanel('GUI is ready.');
}
