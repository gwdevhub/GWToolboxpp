import { defineCollection, z } from 'astro:content';
import { glob } from 'astro/loaders';

const docs = defineCollection({
  loader: glob({ pattern: '**/*.{md,mdx}', base: './src/content/docs' }),
  schema: z.object({
    title: z.string(),
    description: z.string().optional(),
    section: z
      .enum(['overview', 'windows', 'widgets', 'features', 'meta'])
      .default('features'),
    order: z.number().optional(),
    hidden: z.boolean().optional(),
  }),
});

export const collections = { docs };
