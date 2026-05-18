<!DOCTYPE html>
<html>
<head>
    <title>Groenwerf dashboard</title>
    <link rel="stylesheet" href="style.css">
    <script src="https://cdn.jsdelivr.net/npm/@tailwindcss/browser@4"></script>
    <script src="/node_modules/chart.js/dist/chart.umd.min.js"></script>
</head>
<body>

<div class="sidebar">
    <h2 class="text-center">Groenvoorziening</h2>
    <ul>
        <a href="index.php"> <li class="active">Dashboard</li> </a>
        <a href="rapport.php"> <li>Rapport</li> </a>
    </ul>
</div>

<div class="flex p-5 w-full flex-col pl-[220px]">
    <h1 class="px-6 text-2xl font-bold mb-2">Dashboard</h1>
    <div class="w-full grid grid-cols-1 gap-4 p-6">
        <div class="row-span-2 grid grid-cols-1 aspect-16/9 shadow-lg rounded-lg border border-gray-200 p-4">
            <p class="text-3xl row-span-1 font-bold flex items-center justify-center h-auto">Veld 1</p>
            <div id="ChartContainer" class="w-full h-full grid grid-cols-4 gap-0 items-center row-span-5 justify-center">
                <div class="flex flex-col w-full col-span-1 text-5xl justify-items-center">
                    <div class="flex flex-row items-center" style="color:#16a34a">
                        <p class="w-auto">S:</p>
                        <div class="mx-4 text-4xl text-end w-3/5">
                            <p class="">20%</p>
                        </div>
                    </div>
                    <div class="flex flex-row items-center" style="color:#4ade80">
                        <p class="w-auto">A:</p>
                        <div class="mx-4 text-4xl text-end w-3/5">
                            <p class="">20%</p>
                        </div>
                    </div>
                    <div class="flex flex-row items-center" style="color:#a3e635">
                        <p class="w-auto">B:</p>
                        <div class="mx-4 text-4xl text-end w-3/5">
                            <p class="">20%</p>
                        </div>
                    </div>
                    <div class="flex flex-row items-center" style="color:#fbbf24">
                        <p class="w-auto">C:</p>
                        <div class="mx-4 text-4xl text-end w-3/5">
                            <p class="">20%</p>
                        </div>
                    </div>
                    <div class="flex flex-row items-center" style="color:#f97316">
                        <p class="w-auto">D:</p>
                        <div class="mx-4 text-4xl text-end w-3/5">
                            <p class="">20%</p>
                        </div>
                    </div>
                </div>
                <div class="col-span-3 object-contain">
                    <canvas id="pieChart"></canvas>
                </div>
            </div>
        </div>
        <!-- <div class="row-span-2 aspect-16/9 shadow-lg rounded-lg border border-gray-200">
    
        </div> -->
    </div>
</div>  
<script src="PieChart.js"></script>
</body>
</html>