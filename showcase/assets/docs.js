'use strict';

const search = document.getElementById('doc-search');
if (search) {
  const cards = [...document.querySelectorAll('#doc-tiles .tile')];
  const empty = document.getElementById('search-empty');

  search.addEventListener('input', () => {
    const query = search.value.trim().toLowerCase();
    let visible = 0;

    for (const card of cards) {
      const terms = (card.textContent + ' ' + card.dataset.keywords).toLowerCase();
      const show = terms.includes(query);
      card.hidden = !show;
      if (show) visible++;
    }

    empty.style.display = visible ? 'none' : 'block';
  });
}
