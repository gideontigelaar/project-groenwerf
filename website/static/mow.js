document.addEventListener("DOMContentLoaded", () => {
    const startRouteBtn = document.getElementById("startRouteBtn");
    const routeStatus = document.getElementById("routeStatus");

    let fadeOutTimeout, hideTimeout;

    function showStatusAndFade(text, colorClass) {
        routeStatus.textContent = text;
        routeStatus.classList.remove("hidden");
        routeStatus.classList.add(colorClass);

        clearTimeout(fadeOutTimeout);
        clearTimeout(hideTimeout);

        fadeOutTimeout = setTimeout(() => {
            routeStatus.style.transition = "opacity 0.5s ease";
            routeStatus.style.opacity = "0";
            hideTimeout = setTimeout(() => {
                routeStatus.classList.add("hidden");
                routeStatus.style.opacity = "";
                routeStatus.style.transition = "";
            }, 500);
        }, 5000);
    }

    if (startRouteBtn) {
        startRouteBtn.addEventListener("click", async () => {
            const originalHtml = startRouteBtn.innerHTML;
            startRouteBtn.innerHTML = '<i class="ph ph-spinner animate-spin text-lg"></i><span>Laden...</span>';
            startRouteBtn.disabled = true;

            clearTimeout(fadeOutTimeout);
            clearTimeout(hideTimeout);
            routeStatus.style.opacity = "";
            routeStatus.style.transition = "";
            routeStatus.classList.add("hidden");
            routeStatus.className = "mt-4 text-sm font-medium hidden";

            try {
                const res = await fetch("/api/route-link");
                const data = await res.json();

                if (data.status === "success") {
                    showStatusAndFade("Route wordt geopend in Navigator...", "text-brand");
                    // redirect directly to the app link
                    window.location.href = data.link;
                } else if (data.status === "empty") {
                    showStatusAndFade(data.message, "text-zinc-500");
                }
            } catch (e) {
                showStatusAndFade("Kon de route niet genereren. Controleer de verbinding.", "text-[#ef4444]");
            } finally {
                startRouteBtn.innerHTML = originalHtml;
                startRouteBtn.disabled = false;
            }
        });
    }
});