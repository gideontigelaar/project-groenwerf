// dark / light theme controller
(function () {
    const root = document.documentElement;
    const sw = document.getElementById("themeSwitch");
    const label = document.getElementById("themeLabel");

    function current() {
        return root.classList.contains("dark") ? "dark" : "light";
    }

    function apply(theme) {
        root.classList.toggle("dark", theme === "dark");
        root.setAttribute("data-bs-theme", theme);
        localStorage.setItem("theme", theme);
        if (sw) sw.checked = theme === "dark";
        if (label) label.textContent = theme === "dark" ? "Donker" : "Licht";
        window.dispatchEvent(new CustomEvent("themechange", { detail: { theme } }));
    }

    apply(current());

    if (sw) {
        sw.addEventListener("change", () => apply(sw.checked ? "dark" : "light"));
    }
})();

// helper for chart scripts: palette that follows active theme
window.chartTheme = function () {
    const dark = document.documentElement.classList.contains("dark");
    return {
        dark,
        text: dark ? "#a1a1aa" : "#5a6e60",
        grid: dark ? "rgba(255,255,255,0.08)" : "rgba(0,0,0,0.06)",
        ok: "#6aa84f",
        warn: "#f59e0b",
        mow: "#ef4444",
        brand: "#3b6d11",
        track: dark ? "rgba(255,255,255,0.08)" : "#e6e8e0",
    };
};