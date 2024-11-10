let sokuCharacters = [];
let global_state = null;
let json = {};
let Opcodes = {
    "STATE_UPDATE":   0,
    "CARDS_UPDATE":   1,
    "L_SCORE_UPDATE": 2,
    "R_SCORE_UPDATE": 3,
    "L_CARDS_UPDATE": 4,
    "R_CARDS_UPDATE": 5,
    "L_NAME_UPDATE":  6,
    "R_NAME_UPDATE":  7,
    "L_STATS_UPDATE": 8,
    "R_STATS_UPDATE": 9,
}

function pad(n, width, z) {
    z = z || '0';
    n = n + '';
    return n.length >= width ? n : new Array(width - n.length + 1).join(z) + n;
}

async function checkCharacter(id)
{
    if (sokuCharacters[id] !== undefined)
        return;
    sokuCharacters[id] = await (await fetch("/charName/" + id)).text();
}

async function getCharacterImage(id)
{
    await checkCharacter(id);
    return "/internal/data/stand/" + sokuCharacters[id] + ".png"
}

async function getCardImage(charId, cardId)
{
    if (cardId === 21)
        return "/internal/data/infoeffect/cardFaceDown.png"
    if (cardId < 100)
        return "/internal/data/card/common/card" + pad(cardId, 3) + ".png";

    await checkCharacter(charId);
    return "/internal/data/card/" + sokuCharacters[charId] + "/card" + pad(cardId, 3) + ".png";
}

function getSkillImage(charId, skillId)
{
    const imgTag = document.createElement("img");

    return new Promise((resolve, reject) => {
        imgTag.onload = () => {
            const canvasElement = document.createElement("canvas");
            const ctx = canvasElement.getContext("2d");

            canvasElement.width = 32;
            canvasElement.height = 32;

            let imgX = skillId * 32;
            let imgY = 0;

            ctx.drawImage(imgTag, imgX, imgY, 32, 32, 0, 0, canvasElement.width, canvasElement.height);
            resolve(canvasElement.toDataURL());
        };
        imgTag.onerror = () => resolve("/");
        imgTag.setAttribute("src", '/skillSheet/' + charId);
    })
}

function updateStat(charId, id, stats, stat)
{
    let nb = stats[stat];
    let div = document.getElementById(id + stat);

    if (nb === 0) {
        div.style.height = "0";
        return;
    }
    div.style.height = "";
    document.getElementById(id + stat + "lvl").setAttribute("src", "/static/img/LEVEL00" + nb + ".png");
}

async function displayStats(charId, id, stats)
{
    updateStat(charId, id, stats, "rod");
    updateStat(charId, id, stats, "doll");
    updateStat(charId, id, stats, "fan");
    updateStat(charId, id, stats, "grimoire");
    updateStat(charId, id, stats, "drops");

    let entries = Object.entries(stats["skills"]);

    for (let i = entries.length; i < 5; i++)
        document.getElementById(id + "skill" + i).style.height = "0";
    for (const [key, value] of entries) {
        let div = document.getElementById(id + "skill" + (key % entries.length));
        let icon = document.getElementById(id + "skill" + (key % entries.length) + "icon");
        let level = document.getElementById(id + "skill" + (key % entries.length) + "lvl");

        if (value) {
            div.style.height = "";
            icon.setAttribute("src", await getSkillImage(charId, key));
            level.setAttribute("src", "/static/img/LEVEL00" + value + ".png");
        } else if (div) {
            div.style.height = "0";
        }
    }
}

async function displayDeck(id, used, hand, deck, chr)
{
    let i = 0;

    for (let g = 0; i < 20 && g < hand.length; i++) {
        let img = document.getElementById(id + i);
        let src = await getCardImage(chr, hand[g]);

        img.setAttribute("src", src);
        img.className = "hand_card";
        g++;
    }
    for (let g = 0; i < 20 && g < deck.length; i++) {
        let img = document.getElementById(id + i);
        let src = await getCardImage(chr, deck[g]);

        img.setAttribute("src", src);
        img.className = "card";
        g++;
    }
    for (let g = 0; i < 20 && g < used.length; i++) {
        let img = document.getElementById(id + i);
        let src = await getCardImage(chr, used[g]);

        img.setAttribute("src", src);
        img.className = "used_card";
        g++;
    }
    for (; i < 20; i++) {
        let img = document.getElementById(id + i);

        img.className = "unused_card";
    }
}

function checkState()
{
    if (global_state)
        return true;

    fetch('/state').then(r => r.json.then(update));
    return false;
}

async function update(state)
{
    global_state = state;

    let lchr = state.left.character;
    let rchr = state.right.character;
    let round = document.getElementById("roundMarker");

    lChr.setAttribute("src", await getCharacterImage(lchr));
    for (let c of lChr.classList)
        if (/chr\d+/.test(c))
            lChr.classList.remove(c);
    lChr.classList.add("chr" + lchr);
    rChr.setAttribute("src", await getCharacterImage(rchr));
    for (let c of rChr.classList)
        if (/chr\d+/.test(c))
            rChr.classList.remove(c);
    rChr.classList.add("chr" + rchr);
    leftName.textContent = state.left.name;
    rightName.textContent = state.right.name;
    leftScore.textContent = state.left.score + "";
    rightScore.textContent = state.right.score + "";

    if (round)
        round.textContent = state.round;

    await displayDeck("lCard", state.left.used,  state.left.hand,  state.left.deck,  lchr);
    await displayDeck("rCard", state.right.used, state.right.hand, state.right.deck, rchr);
    displayStats(lchr, "l", state.left.stats);
    displayStats(rchr, "r", state.right.stats);
}

async function setPortraits(i) {
    let url = "/internal/data/stand/" + (await (await fetch('/charName/' + i)).text()) + ".png";

    lChr.setAttribute("src", await getCharacterImage(i));
    for (let c of lChr.classList)
        if (/chr\d+/.test(c))
            lChr.classList.remove(c);
    lChr.classList.add("chr" + i);
    rChr.setAttribute("src", await getCharacterImage(i));
    for (let c of rChr.classList)
        if (/chr\d+/.test(c))
            rChr.classList.remove(c);
    rChr.classList.add("chr" + i);
}

async function updateDecks(decks)
{
    if (!checkState())
        return;

    await displayDeck("lCard", decks.left.used,  decks.left.hand,  decks.left.deck,  global_state.left.character);
    await displayDeck("rCard", decks.right.used, decks.right.hand, decks.right.deck, global_state.right.character);

    global_state.left.deck  = decks.left.deck;
    global_state.left.used  = decks.left.used;
    global_state.left.hand  = decks.left.hand;
    global_state.right.deck = decks.right.deck;
    global_state.right.used = decks.right.used;
    global_state.right.hand = decks.right.hand;
}

function updateLeftScore(newScore) {
    if (!checkState())
        return;
    leftScore.textContent = newScore + "";
    global_state.left.score = newScore;
}

function updateRightScore(newScore)
{
    if (!checkState())
        return;
    rightScore.textContent = newScore + "";
    global_state.right.score = newScore;
}

function updateLeftDeck(deck)
{
    if (!checkState())
        return;
    displayDeck("lCard", deck.used,  deck.hand,  deck.deck,  global_state.left.character);
    global_state.left.deck  = deck.deck;
    global_state.left.used  = deck.used;
    global_state.left.hand  = deck.hand;
}

function updateRightDeck(deck)
{
    if (!checkState())
        return;
    displayDeck("rCard", deck.used, deck.hand, deck.deck, global_state.right.character);
    global_state.right.deck = deck.deck;
    global_state.right.used = deck.used;
    global_state.right.hand = deck.hand;
}

function updateLeftStats(stats)
{
    if (!checkState())
        return;
    displayStats(global_state.left.character, "l", stats);
    global_state.left.stats = stats;
}

function updateRightStats(stats)
{
    if (!checkState())
        return;
    displayStats(global_state.right.character, "r", stats);
    global_state.right.stats = stats;
}

function updateLeftName(newName)
{
    if (!checkState())
        return;
    leftName.textContent = newName;
    global_state.left.name = newName;
}

function updateRightName(newName)
{
    if (!checkState())
        return;
    rightName.textContent = newName;
    global_state.right.name = newName;
}

function handleWebSocketMsg(event)
{
    let json = JSON.parse(event.data);
    let data = json.d;

    console.log(json);
    switch (json.o) {
    case Opcodes.STATE_UPDATE:
        return update(data);
    case Opcodes.CARDS_UPDATE:
        return updateDecks(data);
    case Opcodes.L_SCORE_UPDATE:
        return updateLeftScore(data);
    case Opcodes.R_SCORE_UPDATE:
        return updateRightScore(data);
    case Opcodes.L_CARDS_UPDATE:
        return updateLeftDeck(data);
    case Opcodes.R_CARDS_UPDATE:
        return updateRightDeck(data);
    case Opcodes.L_NAME_UPDATE:
        return updateLeftName(data);
    case Opcodes.R_NAME_UPDATE:
        return updateRightName(data);
    case Opcodes.L_STATS_UPDATE:
        return updateLeftStats(data);
    case Opcodes.R_STATS_UPDATE:
        return updateRightStats(data);
    }
}

function initWebSocket() {
    let url = "ws://" + window.location.href.split('/')[2] + "/chat";

    console.log("Connecting to " + url);

    let sock = new WebSocket(url);

    sock.onmessage = handleWebSocketMsg;
    sock.onclose = (e) => {
        console.warn(e);
        global_state = null;
        setTimeout(initWebSocket, 10000);
    };
    sock.onerror = console.error;
}
initWebSocket();