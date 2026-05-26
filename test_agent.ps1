function Invoke-AstraRequest {
    param ([string]$Message)

    $pipe = New-Object System.IO.Pipes.NamedPipeClientStream(".", "astra_engine_pipe", [System.IO.Pipes.PipeDirection]::InOut)
    try {
        # 5-second timeout to handle large files gracefully
        $pipe.Connect(5000) 
    } catch {
        Write-Warning "Failed to connect. Is the ASTRA daemon running?"
        return $null
    }

    $writer = New-Object System.IO.StreamWriter($pipe)
    $reader = New-Object System.IO.StreamReader($pipe)

    $writer.Write($Message)
    $writer.Flush()
    
    $response = $reader.ReadToEnd()
    $pipe.Close()
    
    return $response
}

Clear-Host
Write-Host "ASTRA CLI Client" -ForegroundColor Cyan
Write-Host "----------------" -ForegroundColor DarkGray

$targetPath = Read-Host "Enter project path to scan"

if (Test-Path $targetPath) {
    # Fetch raw files
    $allFiles = Get-ChildItem -Path $targetPath -Include *.cpp, *.h, *.hpp, *.c, *.ts, *.js, *.py, *.java, *.cs, *.go -Recurse -ErrorAction SilentlyContinue
    
    # Filter out build artifacts, package managers, and version control
    $cleanFiles = $allFiles | Where-Object { 
        $_.FullName -notmatch "\\node_modules\\" -and 
        $_.FullName -notmatch "\\dist\\" -and 
        $_.FullName -notmatch "\\build\\" -and
        $_.FullName -notmatch "\\\.git\\" -and
        $_.FullName -notmatch "\\__pycache__\\" -and
        $_.FullName -notmatch "\\\.venv\\"
    }
    
    Write-Host "Found $($cleanFiles.Count) source files. Starting ingestion..." -ForegroundColor Yellow
    
    foreach ($file in $cleanFiles) {
        Write-Host "Parsing: $($file.Name)" -ForegroundColor DarkGray
        
        $content = Get-Content $file.FullName -Raw
        
        if (-not [string]::IsNullOrWhiteSpace($content)) {
            $response = Invoke-AstraRequest -Message "ANALYZE:$($file.Extension)|$content"
            
            # 20ms OS buffer release window
            Start-Sleep -Milliseconds 20
        }
    }
    Write-Host "Index build complete." -ForegroundColor Green
} else {
    Write-Host "Path not found." -ForegroundColor Red
    exit
}

Write-Host "`nReady for queries. Type 'exit' to quit." -ForegroundColor DarkGray

# Interactive Query Loop
while ($true) {
    $query = Read-Host "`nastra>"
    
    if ($query.ToLower() -eq 'exit' -or [string]::IsNullOrWhiteSpace($query)) { 
        break 
    }
    
    $result = Invoke-AstraRequest -Message "QUERY:$($query.Trim())"
    if ($result) {
        Write-Host $result -ForegroundColor Magenta
    }
}