// Main orchestrator function
function setupImageCustomization() {
    const settingsButton = initializeSettingsUI();
    setupImageUpload();
    setupDistanceScale();
    const calcModal = setupScaleCalculator();
    const modal = setupSettingsModal(settingsButton);
    setupConfigManagement();
    setupModalWindowHandlers(modal, calcModal);
}

// 1. Creates and adds settings button to sidebar
function initializeSettingsUI() {
    const metadataSection = document.querySelector('.metadata');
    
    const settingsButtonDiv = document.createElement('div');
    settingsButtonDiv.className = 'sidebar-button-div';
    settingsButtonDiv.style.marginTop = '10px';
    
    const settingsButton = document.createElement('button');
    settingsButton.id = 'open-settings';
    settingsButton.className = 'point-button';
    settingsButton.textContent = 'Settings';
    settingsButton.style.flexGrow = '1';
    
    settingsButtonDiv.appendChild(settingsButton);
    metadataSection.appendChild(settingsButtonDiv);
    
    return settingsButton;
}

// 2. Sets up image upload functionality
function setupImageUpload() {
    const uploadButton = document.getElementById('upload-image-settings');
    const fileInput = document.getElementById('image-upload');
    
    uploadButton.addEventListener('click', function() {
        fileInput.click();
    });
    
    fileInput.addEventListener('change', handleImageUpload);
}

function handleImageUpload(e) {
    if (e.target.files && e.target.files[0]) {
        const reader = new FileReader();
        reader.onload = function(event) {
            const newImg = new Image();
            newImg.onload = function() {
                resizeCanvasToImage(newImg);
                img = newImg;
                get_anchors_and_refresh_canvas();
                logOnLoggingPanel("New background image loaded");
            };
            newImg.src = event.target.result;
        };
        reader.readAsDataURL(e.target.files[0]);
    }
}

// 3. Sets up distance scale controls
function setupDistanceScale() {
    const scaleInput = document.getElementById('distance-scale-settings');
    const updateButton = document.getElementById('update-scale-settings');
    const calcButton = document.getElementById('calculate-scale-settings');
    
    scaleInput.value = distanceScale;
    
    scaleInput.addEventListener('change', updateDistanceScale);
    updateButton.addEventListener('click', updateDistanceScale);
    calcButton.addEventListener('click', openScaleCalculator);
}

function updateDistanceScale() {
    const scaleInput = document.getElementById('distance-scale-settings');
    distanceScale = parseFloat(scaleInput.value);
    logOnLoggingPanel(`Distance scale set to ${distanceScale}`);
    startdraw(xprev, yprev);
}

// 4. Sets up scale calculator modal and functionality
function setupScaleCalculator() {
    const calcModal = document.getElementById('scale-calculator-modal');
    const calcScaleButton = document.getElementById('calc-scale-button');
    const applyScaleButton = document.getElementById('apply-scale-button');
    const closeScaleModalButton = document.getElementById('close-scale-modal');
    
    calcScaleButton.addEventListener('click', calculateScale);
    applyScaleButton.addEventListener('click', applyCalculatedScale);
    closeScaleModalButton.addEventListener('click', () => {
        calcModal.style.display = 'none';
    });
    
    return calcModal;
}

function openScaleCalculator() {
    document.getElementById('pixel-distance').value = '';
    document.getElementById('real-distance').value = '';
    document.getElementById('calculated-scale').textContent = '--';
    document.getElementById('apply-scale-button').disabled = true;
    document.getElementById('scale-calculator-modal').style.display = 'block';
}

function calculateScale() {
    const pixelDistance = parseFloat(document.getElementById('pixel-distance').value);
    const realDistance = parseFloat(document.getElementById('real-distance').value);
    const resultElement = document.getElementById('calculated-scale');
    const applyScaleButton = document.getElementById('apply-scale-button');
    
    if (isNaN(pixelDistance) || isNaN(realDistance) || pixelDistance <= 0 || realDistance <= 0) {
        resultElement.textContent = 'Invalid input';
        resultElement.style.color = '#cf6679';
        applyScaleButton.disabled = true;
        return;
    }
    
    const calculatedScale = (pixelDistance / realDistance).toFixed(2);
    resultElement.textContent = calculatedScale;
    resultElement.style.color = '#03dac6';
    applyScaleButton.disabled = false;
}

function applyCalculatedScale() {
    const calculatedScale = parseFloat(document.getElementById('calculated-scale').textContent);
    const scaleInput = document.getElementById('distance-scale-settings');
    scaleInput.value = calculatedScale;
    distanceScale = calculatedScale;
    logOnLoggingPanel(`Distance scale set to ${calculatedScale}`);
    startdraw(xprev, yprev);
    document.getElementById('scale-calculator-modal').style.display = 'none';
}

// 5. Sets up settings modal controls
function setupSettingsModal(settingsButton) {
    const modal = document.getElementById('settings-modal');
    const closeModalButton = document.getElementById('close-settings-modal');
    
    settingsButton.addEventListener('click', () => {
        modal.style.display = 'block';
    });
    
    closeModalButton.addEventListener('click', () => {
        modal.style.display = 'none';
    });
    
    return modal;
}

// 6. Sets up configuration save/load functionality
function setupConfigManagement() {
    const saveConfigButton = document.getElementById('save-config-settings');
    const loadConfigButton = document.getElementById('load-config-settings');
    
    saveConfigButton.addEventListener('click', saveConfiguration);
    loadConfigButton.addEventListener('click', loadConfiguration);
}

function saveConfiguration() {
    const configData = JSON.stringify({
        uwblocationmonitor: true, // Identifier field for validation
        anchorPositions: anchorPositions,
        distanceScale: distanceScale
    }, null, 2);

    const blob = new Blob([configData], { type: 'application/json' });
    const url = URL.createObjectURL(blob);

    const a = document.createElement('a');
    a.href = url;
    a.download = 'floor_config_' + new Date().toISOString().split('T')[0] + '.json';
    document.body.appendChild(a);
    a.click();

    setTimeout(() => {
        document.body.removeChild(a);
        URL.revokeObjectURL(url);
    }, 0);

    logOnLoggingPanel(`Configuration saved with distance scale: ${distanceScale}`, "INFO");
}

function loadConfiguration() {
    const input = document.createElement('input');
    input.type = 'file';
    input.accept = '.json';

    input.onchange = function(event) {
        if (event.target.files[0]) {
            const reader = new FileReader();
            reader.onload = processConfigFile;
            reader.readAsText(event.target.files[0]);
        }
    };

    input.click();
}

function processConfigFile(e) {
    try {
        const loadedData = JSON.parse(e.target.result);

        // Validate file format
        if (!loadedData.uwblocationmonitor) {
            logOnLoggingPanel("Invalid configuration file format", "ERROR");
            return;
        }

        // Load anchor positions
        if (!loadedData.anchorPositions) {
            logOnLoggingPanel("No anchor positions found in configuration", "ERROR");
            return;
        }
        anchorPositions = loadedData.anchorPositions;

        // Load distance scale
        if (loadedData.distanceScale && !isNaN(parseFloat(loadedData.distanceScale))) {
            distanceScale = parseFloat(loadedData.distanceScale);
            document.getElementById('distance-scale-settings').value = distanceScale;
            logOnLoggingPanel(`Distance scale loaded: ${distanceScale}`, "INFO");
        } else {
            logOnLoggingPanel("No valid distance scale found in configuration", "WARNING");
        }

        // Sync with server and redraw
        syncServerAnchorPositions('PUT').then(() => {
            logOnLoggingPanel(`Configuration loaded with ${Object.keys(anchorPositions).length} anchors`);
            get_anchors_and_refresh_canvas();
        });
    } catch (error) {
        logOnLoggingPanel(`Error loading configuration: ${error.message}`, "ERROR");
    }
}

// 7. Sets up modal close on outside click
function setupModalWindowHandlers(modal, calcModal) {
    window.addEventListener('click', function(event) {
        if (event.target === calcModal) {
            calcModal.style.display = 'none';
        }
        if (event.target === modal) {
            modal.style.display = 'none';
        }
    });
}