package com.compiler.backend.parser;

import com.compiler.backend.model.TokenRecord;

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

public class TokenParser {

    public List<TokenRecord> parse(String filePath) throws IOException {
        List<TokenRecord> tokens = new ArrayList<>();

        try (BufferedReader reader = new BufferedReader(new FileReader(filePath))) {
            String line;
            int index = 0;
            while ((line = reader.readLine()) != null) {
                line = line.trim();
                if (line.isEmpty() || line.startsWith("#")) {
                    continue;
                }
                String[] parts = line.split("\\s+", 4);
                if (parts.length >= 2) {
                    TokenRecord token = new TokenRecord();
                    token.setIndex(index++);
                    token.setType(parts[0]);
                    token.setLexeme(parts[1]);
                    if (parts.length >= 3) {
                        token.setLine(Integer.parseInt(parts[2]));
                    }
                    if (parts.length >= 4) {
                        token.setCol(Integer.parseInt(parts[3]));
                    }
                    tokens.add(token);
                }
            }
        }
        return tokens;
    }
}
