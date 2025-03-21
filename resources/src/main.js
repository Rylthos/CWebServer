window.addEventListener("load", () => {
    let input = document.querySelector("#input");
    input.addEventListener("input", () => {
        let result = document.querySelector("#result");
        result.textContent = input.value;
    });
})
