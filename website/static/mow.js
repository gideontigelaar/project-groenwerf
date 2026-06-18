document.addEventListener("DOMContentLoaded", () => {
    const startRouteBtn = document.getElementById("startRouteBtn");
    const routeStatus = document.getElementById("routeStatus");

    if (startRouteBtn) {
        startRouteBtn.addEventListener("click", async () => {
            const originalHtml = startRouteBtn.innerHTML;
            startRouteBtn.innerHTML = '<i class="ph ph-spinner animate-spin text-lg"></i><span>Laden...</span>';
            startRouteBtn.disabled = true;

            routeStatus.classList.add("hidden");
            routeStatus.className = "mt-4 text-sm font-medium hidden";

            try {
                const res = await fetch("/api/route-link");
                const data = await res.json();

                if (data.status === "success") {
                    routeStatus.textContent = "Route wordt geopend in Navigator...";
                    routeStatus.classList.remove("hidden");
                    routeStatus.classList.add("text-brand");

                    // redirect directly to the app link
                    window.location.href = data.link;
                } else if (data.status === "empty") {
                    routeStatus.textContent = data.message;
                    routeStatus.classList.remove("hidden");
                    routeStatus.classList.add("text-zinc-500");
                }
            } catch (e) {
                routeStatus.textContent = "Kon de route niet genereren. Controleer de verbinding.";
                routeStatus.classList.remove("hidden");
                routeStatus.classList.add("text-[#ef4444]");
            } finally {
                startRouteBtn.innerHTML = originalHtml;
                startRouteBtn.disabled = false;
            }
        });
    }
});