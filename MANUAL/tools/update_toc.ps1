# Actualiza el indice (TOC) de uno o varios .docx con Word, igual que pulsar
# F9 sobre el indice y guardar. Hay que ejecutarlo antes de regenerar el .md
# con pandoc: el .md copia el indice cacheado del .docx, asi que si no se
# actualiza aqui las secciones nuevas no salen en el indice del .md.
#
# Requiere Microsoft Word instalado y los .docx CERRADOS en Word.
#
# Uso:
#   pwsh MANUAL/tools/update_toc.ps1 MANUAL/ES/SD81_Manual_ES.docx MANUAL/EN/SD81_Manual_EN.docx

param([Parameter(Mandatory = $true, ValueFromRemainingArguments = $true)][string[]]$Paths)

$word = New-Object -ComObject Word.Application
$word.Visible = $false
$word.DisplayAlerts = 0
try {
    foreach ($p in $Paths) {
        $full = (Resolve-Path $p).Path
        # Open(FileName, ConfirmConversions, ReadOnly, AddToRecentFiles)
        $doc = $word.Documents.Open($full, $false, $false, $false)
        try {
            foreach ($toc in $doc.TablesOfContents) { $toc.Update() }
            $doc.Save()
            Write-Output "TOC actualizado ($($doc.TablesOfContents.Count)): $full"
        } finally {
            $doc.Close()
        }
    }
} finally {
    $word.Quit()
    [System.Runtime.InteropServices.Marshal]::ReleaseComObject($word) | Out-Null
}
