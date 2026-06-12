(function () {
    const root = document.documentElement;
    const btn = document.getElementById("themeToggleBtn");

    function current() {
        return root.classList.contains("dark") ? "dark" : "light";
    }

    function apply(theme) {
        root.classList.toggle("dark", theme === "dark");
        root.setAttribute("data-bs-theme", theme);
        localStorage.setItem("theme", theme);
        // broadcast theme change to external listeners
        window.dispatchEvent(new CustomEvent("themechange", { detail: { theme } }));
    }

    apply(current());

    if (btn) {
        btn.addEventListener("click", () => {
            apply(current() === "dark" ? "light" : "dark");
        });
    }
})();

// export chart palettes mapped to current theme state
window.chartTheme = function () {
    const dark = document.documentElement.classList.contains("dark");
    return {
        dark,
        text: dark ? "#a1a1aa" : "#5a6e60",
        grid: dark ? "rgba(255,255,255,0.08)" : "rgba(0,0,0,0.06)",
        aplus: "#22c55e",
        a: "#84cc16",
        b: "#eab308",
        c: "#f97316",
        d: "#ef4444",
        brand: "#3b6d11",
    };
};