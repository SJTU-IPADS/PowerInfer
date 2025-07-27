all

exclude_rule 'MD013'  # Line length
exclude_rule 'MD033'  # Inline HTML
exclude_rule 'MD034'  # Bare URL (for now)

rule 'MD026', punctuation: '.,;:!'  # Trailing punctuation in header (& in this case)
rule 'MD029', style: :ordered
rule 'MD007', indent: 2
