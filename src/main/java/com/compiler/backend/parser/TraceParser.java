package com.compiler.backend.parser;

import com.compiler.backend.model.TraceEntry;
import com.opencsv.CSVReader;
import com.opencsv.CSVReaderBuilder;
import com.opencsv.exceptions.CsvException;

import java.io.FileReader;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.stream.Collectors;

public class TraceParser {

    public List<TraceEntry> parse(String filePath) throws IOException, CsvException {
        List<TraceEntry> entries = new ArrayList<>();

        try (CSVReader reader = new CSVReaderBuilder(new FileReader(filePath))
                .withCSVParser(new com.opencsv.CSVParserBuilder()
                        .withSeparator('\t')
                        .build())
                .build()) {
            List<String[]> rows = reader.readAll();
            for (int i = 1; i < rows.size(); i++) {
                String[] row = rows.get(i);
                TraceEntry entry = new TraceEntry();
                entry.setStep(Integer.parseInt(row[0]));
                entry.setState(Integer.parseInt(row[1]));
                entry.setLookaheadId(Integer.parseInt(row[2]));
                entry.setLookahead(row[3]);
                entry.setAction(row[4]);
                entry.setProductionId(Integer.parseInt(row[5]));
                entry.setStateStack(parseStateStack(row[6]));
                entry.setSymbolStack(parseSymbolStack(row[7]));
                entry.setInputIndex(Integer.parseInt(row[8]));
                entries.add(entry);
            }
        }
        return entries;
    }

    private List<Integer> parseStateStack(String raw) {
        return Arrays.stream(raw.trim().split("\\s+"))
                .filter(s -> !s.isEmpty())
                .map(Integer::parseInt)
                .collect(Collectors.toList());
    }

    private List<String> parseSymbolStack(String raw) {
        if (raw == null || raw.trim().isEmpty()) {
            return new ArrayList<>();
        }
        return Arrays.stream(raw.trim().split("\\s+"))
                .filter(s -> !s.isEmpty())
                .collect(Collectors.toList());
    }
}
