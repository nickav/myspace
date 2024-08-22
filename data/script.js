var invert = false;

const up = (i) => {
    try { localStorage.setItem('invert', i ? 1 : 0); } catch(err) {}

    document.body.classList.toggle('load', false);
    document.body.classList.toggle('invert', i);
    document.body.classList.toggle('load', true);
    invert = i;
};

function toggle()
{
    up(!invert);
}

if (window.matchMedia)
{
    try { i = localStorage.getItem('invert'); } catch(err) {}
    if (i === null) i = !window.matchMedia('(prefers-color-scheme: dark)').matches;
    else i = !!Number(i);

    up(i);

    window.matchMedia('(prefers-color-scheme: dark)').addEventListener('change', e => {
        up(!e.matches, true);
    });
}

window.addEventListener('load', () => {
    document.getElementsByTagName('html')[0].className += 'load';
});
