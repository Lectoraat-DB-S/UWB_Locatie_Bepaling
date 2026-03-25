function updateTimer() {
    const d = new Date();
    const date_string = d.toLocaleDateString('nl-NL', {
        year: 'numeric',
        month: '2-digit',
        day: '2-digit',
        hour: '2-digit',
        minute: '2-digit',
        second: '2-digit'
    });
    document.getElementById("timing").innerHTML = "Current time: " + date_string;
}

function getServerStatus() {
    // Create an AbortController with signal
    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), 1000);

    let status = document.getElementById('server-status');

    fetch('http://localhost:5000/api/status', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        signal: controller.signal
    })
    .then(response => {
        clearTimeout(timeoutId);
        status.innerHTML = "Connected";
        status.style.color = "green";
    })
    .catch(error => {
        status.innerHTML = "Disconnected";
        status.style.color = "red";
    });
}

// Run timer update every half second
setInterval(() => {
    updateTimer();
}, 500);

// Run server status check every second
setInterval(() => {
    getServerStatus();
}, 1000);

// Initial call to avoid waiting a second for first update
updateTimer();
getServerStatus();