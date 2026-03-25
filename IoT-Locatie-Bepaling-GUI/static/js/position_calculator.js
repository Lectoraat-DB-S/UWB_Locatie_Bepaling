function calculateTagPosition(tagData, tagHeight = 0) {
    // console.time('calculateTagPosition');
    // Determine how to access anchor positions
    let anchorData = anchorPositions;
    if (anchorPositions && anchorPositions.data && typeof anchorPositions.data === 'object') {
        anchorData = anchorPositions.data;
    }

    // Get anchors with known positions
    const relevantAnchors = tagData.anchors.filter(anchor => {
        return anchor.mac in anchorPositions && anchor.distance > 0;
    });

    if (relevantAnchors.length < 3) {
        return {x: -1, y: -1};
    }

    // Use better initial guess based on target area
    let estimatedPos = {x: 500, y: 500};
    const maxIterations = 20;

    for (let i = 0; i < maxIterations; i++) {
        const J = [];
        const residuals = [];

        for (const anchor of relevantAnchors) {
            const anchorPos = anchorData[anchor.mac];
            if (!anchorPos) continue;

            const dx = estimatedPos.x - anchorPos.x;
            const dy = estimatedPos.y - anchorPos.y;

            // Account for height difference using Pythagorean theorem
            const anchorHeight = anchorPos.z || 0;
            const heightDiff = anchorHeight - tagHeight;

            // Apply scaling to measured distance
            const measuredDistance = anchor.distance * distanceScale;

            // Calculate horizontal distance using Pythagorean theorem
            let horizontalDistance;
            if (measuredDistance > Math.abs(heightDiff)) {
                horizontalDistance = Math.sqrt(Math.pow(measuredDistance, 2) - Math.pow(heightDiff, 2));
            } else {
                // If height difference is greater than measured distance, use a small value
                horizontalDistance = 0.1;  // Small non-zero value to avoid numerical issues
            }

            const calculatedDistance = Math.sqrt(dx*dx + dy*dy);

            const r = calculatedDistance > 0.1 ? calculatedDistance : 0.1;
            J.push([dx/r, dy/r]);
            residuals.push(horizontalDistance - calculatedDistance);
        }

        if (J.length === 0) continue;

        const JT = transpose(J);
        const JTJ = multiply(JT, J);
        const JTr = multiplyMatrixVector(JT, residuals);
        const delta = solveLinearSystem(JTJ, JTr);

        if (!delta) break;

        estimatedPos.x += delta[0];
        estimatedPos.y += delta[1];

        if (Math.abs(delta[0]) < 0.1 && Math.abs(delta[1]) < 0.1) break;
    }

    // console.timeEnd('calculateTagPosition');
    return {
        x: Math.round(estimatedPos.x),
        y: Math.round(estimatedPos.y)
    };
}

// Matrix transpose operation
function transpose(matrix) {
    const rows = matrix.length;
    const cols = matrix[0].length;
    const result = Array(cols).fill().map(() => Array(rows).fill(0));

    for (let i = 0; i < rows; i++) {
        for (let j = 0; j < cols; j++) {
            result[j][i] = matrix[i][j];
        }
    }
    return result;
}

// Matrix multiplication
function multiply(A, B) {
    const rowsA = A.length;
    const colsA = A[0].length;
    const colsB = B[0].length;
    const result = Array(rowsA).fill().map(() => Array(colsB).fill(0));

    for (let i = 0; i < rowsA; i++) {
        for (let j = 0; j < colsB; j++) {
            for (let k = 0; k < colsA; k++) {
                result[i][j] += A[i][k] * B[k][j];
            }
        }
    }
    return result;
}

// Matrix-vector multiplication
function multiplyMatrixVector(A, v) {
    const rows = A.length;
    const cols = A[0].length;
    const result = Array(rows).fill(0);

    for (let i = 0; i < rows; i++) {
        for (let j = 0; j < cols; j++) {
            result[i] += A[i][j] * v[j];
        }
    }
    return result;
}

// Solve 2×2 linear system using Cramer's rule
function solveLinearSystem(A, b) {
    // For 2×2 system, we can use direct formula
    const det = A[0][0] * A[1][1] - A[0][1] * A[1][0];

    if (Math.abs(det) < 1e-10) { // Close to zero determinant
        return null;
    }

    const x = (b[0] * A[1][1] - b[1] * A[0][1]) / det;
    const y = (A[0][0] * b[1] - A[1][0] * b[0]) / det;

    return [x, y];
}
