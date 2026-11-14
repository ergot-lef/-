#!/usr/bin/env python3
"""Prépare le livre lu par l'Écrivain (texte Project Gutenberg, domaine public).
Retire l'en-tête et la licence Gutenberg, recolle les lignes coupées,
garde les paragraphes. Usage : prepare_livre.py entree.txt sortie.txt"""
import re
import sys

raw = open(sys.argv[1], encoding="utf-8", errors="replace").read().replace("\r", "")
start = re.search(r"\*\*\* ?START OF (THE|THIS) PROJECT GUTENBERG.*\n", raw)
end = re.search(r"\*\*\* ?END OF (THE|THIS) PROJECT GUTENBERG", raw)
body = raw[start.end() if start else 0: end.start() if end else len(raw)]

paras = [re.sub(r"\s+", " ", p).strip() for p in re.split(r"\n\s*\n", body)]
paras = [p for p in paras if len(p) > 1]
text = "\n".join(paras) + "\n"
text = text.replace(" ", " ").replace(" ", " ")
open(sys.argv[2], "w", encoding="utf-8").write(text)
print(len(text), "caractères,", len(paras), "paragraphes")
