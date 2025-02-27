window.onload = function() {
    const canvas = document.getElementById('myCanvas');
    const ctx = canvas.getContext('2d');
    const img = document.getElementById('myImage');

    img.onload = function() {
        ctx.drawImage(img, 0, 0, canvas.width, canvas.height);

        // Drawing on top of the image
        ctx.fillStyle = 'red';
        ctx.font = '30px Arial';
        ctx.fillText('Hello World', 50, 50);
    };
};
