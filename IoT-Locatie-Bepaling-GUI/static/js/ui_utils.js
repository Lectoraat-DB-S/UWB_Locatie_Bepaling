function logOnLoggingPanel(message, level = 'INFO') {
    let logPanel = document.getElementById('uwb-log-content');

    let newLog = document.createElement('p');

    const d = new Date();
    const date_string = d.toLocaleTimeString('nl-NL', {
        hour: '2-digit',
        minute: '2-digit',
        second: '2-digit'
    });

    newLog.innerHTML = `[${date_string} ${level}] ${message}`;

    // add child to top of the log panel
    logPanel.insertBefore(newLog, logPanel.firstChild);

    // remove last child if there are more than 10 logs
    if (logPanel.childElementCount > 10) {
        logPanel.removeChild(logPanel.lastChild);
    }
}

function updateCurrentSelectedPoint(x, y) {
    let selectedPointP = document.getElementById('selected-point');

    // Set text and attributes for selected point,
    // attributes are used for easy loading of saved data later

    selectedPointP.innerHTML = x + ", " + y;  // example: "100, 200"
    selectedPointP.setAttribute('m-pos-x', x);
    selectedPointP.setAttribute('m-pos-y', y);
}
