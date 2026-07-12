#!/usr/bin/env bash
# Compile les trois documents du projet VESPA et les publie dans docs/.
#
#   Prérequis : une distribution LaTeX avec babel-french, biblatex, tcolorbox, listings.
#     Debian/Ubuntu :  sudo apt install texlive-full latexmk
#
#   Usage :  ./build.sh          (compile tout)
#            ./build.sh clean    (supprime les fichiers auxiliaires)

set -euo pipefail
cd "$(dirname "$0")"

DOCS=(Rapport_technique Difficultes_techniques Bibliographie)

if [[ "${1:-}" == "clean" ]]; then
    latexmk -C "${DOCS[@]/%/.tex}" 2>/dev/null || true
    rm -f ./*.aux ./*.bbl ./*.blg ./*.log ./*.out ./*.toc ./*.fls ./*.fdb_latexmk
    echo "Nettoyé."
    exit 0
fi

for doc in "${DOCS[@]}"; do
    echo "==> Compilation de ${doc}..."
    # latexmk gère lui-même les passes multiples (table des matières, bibliographie).
    latexmk -pdf -interaction=nonstopmode -halt-on-error "${doc}.tex"
    cp "${doc}.pdf" ../
    echo "    -> docs/${doc}.pdf"
done

echo
echo "Terminé. Les trois PDF sont publiés dans docs/."
