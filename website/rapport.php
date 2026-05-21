<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Groenwerf dashboard</title>

    <!-- Bootstrap -->
    <link
        href="https://cdn.jsdelivr.net/npm/bootstrap@5.3.8/dist/css/bootstrap.min.css"
        rel="stylesheet"
    >

    <!-- Tailwind -->
    <script src="https://cdn.jsdelivr.net/npm/@tailwindcss/browser@4"></script>

    <link href="style.css" rel="stylesheet">
    <script src="/node_modules/chart.js/dist/chart.umd.min.js"></script>
</head>

<body>
    <!-- Sidebar -->
    <div class="sidebar shrink-0">
        <h2>Groenvoorziening</h2>

        <ul>
            <a href="index.php">
                <li>Dashboard</li>
            </a>

            <a href="rapport.php">
                <li class="active">Rapport</li>
            </a>
        </ul>
    </div>

    <div class="main-content overflow-x-hidden overflow-y-scroll">
        <div class="m-5 h-fit">
            <form class="form-inline flex flex-row w-100">
                <input 
                  class="form-control mr-sm-2 rounded-r-none!" 
                  type="search" 
                  placeholder="Search" 
                  aria-label="Search"
                  style="background-color: #1f3d2b; border-color: #1f3d2b; color: #fff;"
                >
                <select 
                    id="sortSelect"
                    class="form-select rounded-none!"
                    style="background-color: #1f3d2b; border-color: #1f3d2b; color: #fff;"
                >
                    <option value="measured_at">Time</option>
                    <option value="tof_mm">Grass Height (TOF)</option>
                    <option value="sonic_mm">Grass Height (Sonic)</option>
                    <option value="longitude">Longitude</option>
                    <option value="latitude">Latitude</option>
                </select>
                <button 
                  class="btn my-2 my-sm-0 rounded-l-none!" 
                  type="submit"
                  style="background-color: #1f3d2b; border-color: #1f3d2b; color: #fff;"
                >
                  Search
                </button>
            </form>
        </div>
    
        <div class="h-15 m-5 bg-gray-200 rounded-2xl flex flex-row">
            <table class="table table-striped table-hover mb-0">
                <thead style="background-color: #1f3d2b; color: white;">
                    <tr>
                        <th>Grass Height (TOF)</th>
                        <th>Grass Height (Sonic)</th>
                        <th>Address</th>
                        <th></th>
                        <th>Measured at</th>
                    </tr>
                </thead>
                <tbody id="dataTableBody">
                    <tr><td colspan="5" class="text-center">Loading...</td></tr>
                </tbody>
            </table>
        </div>
    </div>
    <!-- Bootstrap JS -->
    <script src="https://cdn.jsdelivr.net/npm/bootstrap@5.3.8/dist/js/bootstrap.bundle.min.js"></script>
    <script src="fetchdata.js"></script>
</body>
</html>