function saveConfig(event) {
    // Stop form submition
    event.preventDefault();

    // Récupère le formulaire
    var formEl = document.querySelector('form');

    var xhttp = new XMLHttpRequest();

    xhttp.onreadystatechange = function () {
        if (event.readyState === 4) {
            if (event.status === 200) {
                console.log('youpi');
            } else {
                console.log('Une erreur est survenue');
            }
        }
    };

    xhttp.open('post', formEl.action, true);
    xhttp.send(new FormData(formEl));
}

