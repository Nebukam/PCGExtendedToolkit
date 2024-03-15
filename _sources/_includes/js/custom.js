var cards = document.querySelectorAll(".card");
for(let i = 0; i < cards.length; i++){ cards[i].style.transform = `rotate(${(Math.random()-Math.random()) * 1.5 }deg)`; }
var nav = document.querySelectorAll(".nav-list-item");

var links = document.links;

for (var i = 0, linksLength = links.length; i < linksLength; i++) {
    if (links[i].hostname != window.location.hostname) {
        links[i].target = '_blank';
    }
}

var kbs = document.querySelectorAll(".kb");
console.log(kbs);
for(let i = 0; i < kbs.length; i++){ 
    var kb = kbs[i];
    var att = kb.getAttribute("shortcut"); 
    var html = [];
    att = att.split(' ');
    att.forEach(element => {
        html.push('<span class="kbkey">'+element+'</span>');
    });
    kb.innerHTML = html.join('+');
}

function ToClipboard(element) {
    var text = element.getElementsByClassName("dl-content")[0].innerText;
    navigator.clipboard.writeText(text);
    alert("Copied the text: " + text);
  }