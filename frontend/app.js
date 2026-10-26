const API_BASE = 'http://localhost:8080/api';
let chartInstance = null;

// On Page Load: Check if C++ Winsock Server is reachable
window.addEventListener('DOMContentLoaded', checkServerStatus);

async function checkServerStatus() {
    const dot = document.getElementById('statusDot');
    const text = document.getElementById('statusText');

    try {
        const res = await fetch(`${API_BASE}/simulate`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ reference_string: [1], frames_count: 1, algorithm: 'FIFO' })
        });

        if (res.ok) {
            dot.className = 'w-2.5 h-2.5 rounded-full bg-emerald-500';
            text.innerText = 'C++ Backend Connected';
        } else {
            throw new Error('Server error');
        }
    } catch (err) {
        dot.className = 'w-2.5 h-2.5 rounded-full bg-rose-500';
        text.innerText = 'C++ Backend Offline';
    }
}

// Generate Random Reference String
function generateRandomRef() {
    const length = 15;
    const randomArray = Array.from({ length }, () => Math.floor(Math.random() * 8));
    document.getElementById('refString').value = randomArray.join(', ');
}

// Parse inputs from HTML
function getInputs() {
    const frameCount = parseInt(document.getElementById('frames').value);
    const rawRefStr = document.getElementById('refString').value;
    const refArray = rawRefStr.split(',')
        .map(x => parseInt(x.trim()))
        .filter(x => !isNaN(x));

    if (isNaN(frameCount) || frameCount < 1) {
        alert('Please enter a valid frame count (>= 1).');
        return null;
    }
    if (refArray.length === 0) {
        alert('Please enter a valid reference string.');
        return null;
    }

    return { frameCount, refArray };
}

// 1. Run Single Algorithm Simulation
async function runSimulation() {
    const inputs = getInputs();
    if (!inputs) return;

    const algorithm = document.getElementById('algo').value;

    try {
        const response = await fetch(`${API_BASE}/simulate`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                frames_count: inputs.frameCount,
                reference_string: inputs.refArray,
                algorithm: algorithm
            })
        });

        if (!response.ok) throw new Error('API Error');

        const data = await response.json();
        
        document.getElementById('compareResultSection').classList.add('hidden');
        document.getElementById('singleResultSection').classList.remove('hidden');

        renderSingleResults(data, inputs.frameCount);
    } catch (err) {
        alert('Failed to connect to C++ Backend. Make sure `server.exe` is running on port 8080.');
        console.error(err);
    }
}

// Render Single Algorithm Table & Metrics
function renderSingleResults(data, frameCount) {
    document.getElementById('statFaults').innerText = data.faults;
    document.getElementById('statHits').innerText = data.hits;
    document.getElementById('statHitRatio').innerText = (data.hit_ratio * 100).toFixed(1) + '%';
    document.getElementById('statFaultRatio').innerText = (data.fault_ratio * 100).toFixed(1) + '%';

    let html = `<table class="matrix-table"><thead><tr>
        <th class="bg-slate-700 text-slate-300">Frame Slot</th>`;

    // Headers = Requested Pages
    data.history.forEach((step, idx) => {
        const badgeClass = step.is_hit ? 'bg-emerald-900/60 text-emerald-300' : 'bg-rose-900/60 text-rose-300';
        html += `<th class="bg-slate-700 text-slate-200">
            <div>Step ${idx + 1}</div>
            <div class="text-base font-bold my-1">${step.page}</div>
            <span class="text-xs px-2 py-0.5 rounded ${badgeClass}">${step.is_hit ? 'HIT' : 'FAULT'}</span>
        </th>`;
    });
    html += `</tr></thead><tbody>`;

    // Rows = Frame Capacity
    for (let f = 0; f < frameCount; f++) {
        html += `<tr><td class="bg-slate-800 text-slate-400 font-medium">Slot ${f + 1}</td>`;

        data.history.forEach(step => {
            const pageVal = step.frames[f] !== undefined ? step.frames[f] : '-';
            const cellClass = step.is_hit ? 'cell-hit' : (pageVal !== '-' ? 'cell-fault' : 'cell-empty');
            const isReplaced = (step.replaced !== -1 && step.frames[f] === step.page);

            html += `<td class="${cellClass}">
                <span class="text-base font-semibold">${pageVal}</span>
                ${isReplaced ? `<span class="replaced-badge">Replaced ${step.replaced}</span>` : ''}
            </td>`;
        });
        html += `</tr>`;
    }

    html += `</tbody></table>`;
    document.getElementById('tableContainer').innerHTML = html;
}

// 2. Run All Algorithms Comparison (FIFO, LRU, Optimal)
async function runComparison() {
    const inputs = getInputs();
    if (!inputs) return;

    try {
        const response = await fetch(`${API_BASE}/compare`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                frames_count: inputs.frameCount,
                reference_string: inputs.refArray
            })
        });

        if (!response.ok) throw new Error('API Error');

        const results = await response.json();

        document.getElementById('singleResultSection').classList.add('hidden');
        document.getElementById('compareResultSection').classList.remove('hidden');

        renderComparisonResults(results);
    } catch (err) {
        alert('Failed to connect to C++ Backend. Make sure `server.exe` is running on port 8080.');
        console.error(err);
    }
}

// Render Comparison Chart and Summary Table
function renderComparisonResults(results) {
    const labels = results.map(r => r.algorithm);
    const faultsData = results.map(r => r.faults);
    const hitsData = results.map(r => r.hits);

    // Chart.js Bar Chart
    const ctx = document.getElementById('comparisonChart').getContext('2d');
    if (chartInstance) chartInstance.destroy();

    chartInstance = new Chart(ctx, {
        type: 'bar',
        data: {
            labels: labels,
            datasets: [
                {
                    label: 'Page Faults',
                    data: faultsData,
                    backgroundColor: 'rgba(244, 63, 94, 0.7)',
                    borderColor: 'rgb(244, 63, 94)',
                    borderWidth: 1
                },
                {
                    label: 'Page Hits',
                    data: hitsData,
                    backgroundColor: 'rgba(16, 185, 129, 0.7)',
                    borderColor: 'rgb(16, 185, 129)',
                    borderWidth: 1
                }
            ]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            scales: {
                y: { beginAtZero: true, grid: { color: '#334155' }, ticks: { color: '#94a3b8' } },
                x: { grid: { display: false }, ticks: { color: '#94a3b8' } }
            },
            plugins: {
                legend: { labels: { color: '#cbd5e1' } }
            }
        }
    });

    // Comparison Table
    let html = `<table class="w-full text-left text-sm text-slate-300">
        <thead class="bg-slate-700 text-slate-200 uppercase text-xs">
            <tr>
                <th class="p-3">Algorithm</th>
                <th class="p-3">Page Faults</th>
                <th class="p-3">Page Hits</th>
                <th class="p-3">Hit Ratio</th>
                <th class="p-3">Fault Ratio</th>
            </tr>
        </thead>
        <tbody class="divide-y divide-slate-700 bg-slate-800">`;

    results.forEach(r => {
        html += `<tr>
            <td class="p-3 font-semibold text-sky-400">${r.algorithm}</td>
            <td class="p-3 font-bold text-rose-400">${r.faults}</td>
            <td class="p-3 font-bold text-emerald-400">${r.hits}</td>
            <td class="p-3">${(r.hit_ratio * 100).toFixed(1)}%</td>
            <td class="p-3">${(r.fault_ratio * 100).toFixed(1)}%</td>
        </tr>`;
    });

    html += `</tbody></table>`;
    document.getElementById('compareTableContainer').innerHTML = html;
}